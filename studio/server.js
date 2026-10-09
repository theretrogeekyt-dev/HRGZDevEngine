/**
 * HRGZDevEngine Studio - Desktop Game Creator & Build Manager Server
 * Lightweight zero-dependency HTTP server that powers the visual studio GUI,
 * handles WAD inspections, manages builds, and coordinates game releases.
 */

const http = require('http');
const fs = require('fs');
const path = require('path');
const { spawn, exec } = require('child_process');
const { parseWad } = require('./wad_parser');
const { detectSystem, buildMac, packageRelease } = require('./builder');

const PORT = process.env.PORT || 4820;
const REPO_ROOT = path.resolve(__dirname, '..');
const UI_DIR = path.join(__dirname, 'ui');
const DEFAULT_PROJECT_FILE = path.join(REPO_ROOT, 'game.json');

let activeGameProcess = null;

// MIME types for static files
const MIME_TYPES = {
    '.html': 'text/html; charset=utf-8',
    '.css': 'text/css; charset=utf-8',
    '.js': 'application/javascript; charset=utf-8',
    '.json': 'application/json; charset=utf-8',
    '.png': 'image/png',
    '.ico': 'image/x-icon',
    '.svg': 'image/svg+xml'
};

function readBody(req) {
    return new Promise((resolve, reject) => {
        let body = '';
        req.on('data', chunk => { body += chunk; });
        req.on('end', () => {
            try {
                resolve(body ? JSON.parse(body) : {});
            } catch (err) {
                reject(err);
            }
        });
        req.on('error', reject);
    });
}

function sendJson(res, statusCode, data) {
    res.writeHead(statusCode, {
        'Content-Type': 'application/json',
        'Access-Control-Allow-Origin': '*'
    });
    res.end(JSON.stringify(data));
}

function getDefaultProject() {
    return {
        title: 'Cyber Assault',
        id: 'cyberassault',
        version: '1.0.0',
        author: 'Indie Studio',
        description: 'A high-octane retro shooter created with HRGZDevEngine.',
        website: 'https://itch.io',
        wadPath: 'doom1.wad',
        gameMode: 'shareware',
        defaultResolution: '426x200',
        scalingMode: 'widescreen_16_9',
        fullscreen: false
    };
}

function loadProject() {
    if (fs.existsSync(DEFAULT_PROJECT_FILE)) {
        try {
            return JSON.parse(fs.readFileSync(DEFAULT_PROJECT_FILE, 'utf8'));
        } catch {
            return getDefaultProject();
        }
    }
    return getDefaultProject();
}

function saveProject(data) {
    fs.writeFileSync(DEFAULT_PROJECT_FILE, JSON.stringify(data, null, 2));
    return data;
}

const server = http.createServer(async (req, res) => {
    const url = new URL(req.url, `http://${req.headers.host}`);

    // CORS preflight
    if (req.method === 'OPTIONS') {
        res.writeHead(204, {
            'Access-Control-Allow-Origin': '*',
            'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
            'Access-Control-Allow-Headers': 'Content-Type'
        });
        return res.end();
    }

    // API Routes
    if (url.pathname === '/api/system' && req.method === 'GET') {
        return sendJson(res, 200, detectSystem());
    }

    if (url.pathname === '/api/project' && req.method === 'GET') {
        return sendJson(res, 200, loadProject());
    }

    if (url.pathname === '/api/project' && req.method === 'POST') {
        try {
            const body = await readBody(req);
            const saved = saveProject(body);
            return sendJson(res, 200, { ok: true, project: saved });
        } catch (err) {
            return sendJson(res, 400, { ok: false, error: err.message });
        }
    }

    if (url.pathname === '/api/wad/inspect' && req.method === 'POST') {
        try {
            const body = await readBody(req);
            const wadPath = body.wadPath ? path.resolve(REPO_ROOT, body.wadPath) : path.join(REPO_ROOT, 'doom1.wad');
            const data = parseWad(wadPath);
            return sendJson(res, 200, { ok: true, wad: data });
        } catch (err) {
            return sendJson(res, 400, { ok: false, error: err.message });
        }
    }

    if (url.pathname === '/api/build' && req.method === 'POST') {
        res.writeHead(200, {
            'Content-Type': 'text/plain; charset=utf-8',
            'Transfer-Encoding': 'chunked',
            'Access-Control-Allow-Origin': '*'
        });

        const sendChunk = (msg) => res.write(msg);

        try {
            const body = await readBody(req);
            const project = body.project || loadProject();
            const target = body.target || 'mac';

            sendChunk(`[STUDIO] Initializing compilation for '${project.title}' (${target})...\n`);

            if (target === 'mac') {
                const appDir = await buildMac(project, sendChunk);
                sendChunk(`\n[COMPLETE] Successfully generated standalone app bundle:\n${appDir}\n`);
            } else {
                sendChunk(`[ERROR] Target '${target}' not supported on this platform.\n`);
            }
            res.end('\n[BUILD_STATUS: SUCCESS]\n');
        } catch (err) {
            sendChunk(`\n[FATAL ERROR] ${err.message}\n`);
            res.end('\n[BUILD_STATUS: FAILED]\n');
        }
        return;
    }

    if (url.pathname === '/api/dist' && req.method === 'POST') {
        res.writeHead(200, {
            'Content-Type': 'text/plain; charset=utf-8',
            'Transfer-Encoding': 'chunked',
            'Access-Control-Allow-Origin': '*'
        });

        const sendChunk = (msg) => res.write(msg);

        try {
            const body = await readBody(req);
            const project = body.project || loadProject();
            const target = body.target || 'mac';

            sendChunk(`[STUDIO] Packaging release archive for '${project.title}' (${target})...\n`);
            const zipPath = await packageRelease(project, target, sendChunk);
            sendChunk(`\n[COMPLETE] Distribution package ready for itch.io / Steam:\n${zipPath}\n`);
            res.end('\n[DIST_STATUS: SUCCESS]\n');
        } catch (err) {
            sendChunk(`\n[FATAL ERROR] ${err.message}\n`);
            res.end('\n[DIST_STATUS: FAILED]\n');
        }
        return;
    }

    if (url.pathname === '/api/run' && req.method === 'POST') {
        try {
            const body = await readBody(req);
            const project = body.project || loadProject();

            if (activeGameProcess && !activeGameProcess.killed) {
                return sendJson(res, 400, { ok: false, error: 'Game is already running!' });
            }

            const appPath = path.join(REPO_ROOT, 'build', 'projects', project.id, 'mac', `${project.title}.app`);
            const macBinary = path.join(appPath, 'Contents', 'MacOS', project.id);

            let execCmd = macBinary;
            let execArgs = [];

            if (!fs.existsSync(macBinary)) {
                // If specific project app not built yet, fallback to general binary or build
                return sendJson(res, 400, { ok: false, error: 'Game executable not found. Please click Build first!' });
            }

            activeGameProcess = spawn(execCmd, execArgs, {
                cwd: path.dirname(macBinary),
                detached: true,
                stdio: 'ignore'
            });

            activeGameProcess.unref();

            activeGameProcess.on('exit', () => {
                activeGameProcess = null;
            });

            return sendJson(res, 200, { ok: true, message: `Game launched (PID: ${activeGameProcess.pid})` });
        } catch (err) {
            return sendJson(res, 500, { ok: false, error: err.message });
        }
    }

    if (url.pathname === '/api/status' && req.method === 'GET') {
        return sendJson(res, 200, {
            running: activeGameProcess !== null && !activeGameProcess.killed
        });
    }

    if (url.pathname === '/api/stop' && req.method === 'POST') {
        if (activeGameProcess) {
            activeGameProcess.kill();
            activeGameProcess = null;
            return sendJson(res, 200, { ok: true, message: 'Game process terminated.' });
        }
        return sendJson(res, 200, { ok: true, message: 'No game process was active.' });
    }

    // Static File Serving
    let filePath = path.join(UI_DIR, url.pathname === '/' ? 'index.html' : url.pathname);
    if (!fs.existsSync(filePath)) {
        filePath = path.join(UI_DIR, 'index.html');
    }

    const ext = path.extname(filePath).toLowerCase();
    const contentType = MIME_TYPES[ext] || 'application/octet-stream';

    fs.readFile(filePath, (err, content) => {
        if (err) {
            res.writeHead(500);
            return res.end(`Server Error: ${err.message}`);
        }
        res.writeHead(200, { 'Content-Type': contentType });
        res.end(content);
    });
});

server.listen(PORT, '127.0.0.1', () => {
    const url = `http://127.0.0.1:${PORT}`;
    console.log(`=======================================================`);
    console.log(`  HRGZDevEngine Studio - Game Creator & Compiler`);
    console.log(`  Dashboard running at: ${url}`);
    console.log(`=======================================================`);

    // Auto-open desktop browser window
    const openCommand = process.platform === 'darwin' ? `open "${url}"` :
                        process.platform === 'win32' ? `start "${url}"` :
                        `xdg-open "${url}"`;
    exec(openCommand, () => {});
});
