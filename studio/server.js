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
const { 
    getEngineRoot, 
    getWorkspaceRoot, 
    resolveWadPath, 
    detectSystem, 
    buildMac, 
    buildWin, 
    buildLinux, 
    packageRelease 
} = require('./builder');

const PORT = process.env.PORT || 4820;
const ENGINE_ROOT = getEngineRoot();
const WORKSPACE_ROOT = getWorkspaceRoot();
const UI_DIR = path.join(__dirname, 'ui');
const DEFAULT_PROJECT_FILE = path.join(WORKSPACE_ROOT, 'game.json');

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
    const def = getDefaultProject();
    saveProject(def);

    // Seed default doom1.wad into workspace if missing
    const wsWad = path.join(WORKSPACE_ROOT, 'doom1.wad');
    const engWad = path.join(ENGINE_ROOT, 'doom1.wad');
    if (!fs.existsSync(wsWad) && fs.existsSync(engWad)) {
        try { fs.copyFileSync(engWad, wsWad); } catch {}
    }
    return def;
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
            const wadPath = resolveWadPath(body.wadPath);
            if (!wadPath || !fs.existsSync(wadPath)) {
                return sendJson(res, 404, { ok: false, error: 'WAD archive not found' });
            }
            const data = parseWad(wadPath);
            return sendJson(res, 200, { ok: true, wad: data, path: wadPath });
        } catch (err) {
            return sendJson(res, 400, { ok: false, error: err.message });
        }
    }

    if (url.pathname === '/api/open-folder' && req.method === 'POST') {
        try {
            const body = await readBody(req);
            const folder = body.folder || 'dist';
            let targetDir = WORKSPACE_ROOT;
            if (folder === 'dist') targetDir = path.join(WORKSPACE_ROOT, 'dist');
            else if (folder === 'build') targetDir = path.join(WORKSPACE_ROOT, 'build');
            else if (folder === 'workspace') targetDir = WORKSPACE_ROOT;
            else if (body.path && fs.existsSync(body.path)) {
                targetDir = fs.statSync(body.path).isDirectory() ? body.path : path.dirname(body.path);
            }

            if (!fs.existsSync(targetDir)) fs.mkdirSync(targetDir, { recursive: true });

            const cmd = process.platform === 'darwin' ? `open "${targetDir}"` :
                        process.platform === 'win32' ? `explorer "${targetDir}"` :
                        `xdg-open "${targetDir}"`;
            exec(cmd, () => {});
            return sendJson(res, 200, { ok: true, path: targetDir });
        } catch (err) {
            return sendJson(res, 500, { ok: false, error: err.message });
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
            } else if (target === 'win') {
                const exePath = await buildWin(project, sendChunk);
                sendChunk(`\n[COMPLETE] Successfully generated Windows executable:\n${exePath}\n`);
            } else if (target === 'linux') {
                const binPath = await buildLinux(project, sendChunk);
                sendChunk(`\n[COMPLETE] Successfully generated Linux binary:\n${binPath}\n`);
            } else if (target === 'all') {
                await buildMac(project, sendChunk);
                await buildWin(project, sendChunk);
                await buildLinux(project, sendChunk);
                sendChunk(`\n[COMPLETE] Successfully compiled all platform binaries!\n`);
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

            let execCmd = null;
            let execArgs = [];
            let execCwd = WORKSPACE_ROOT;

            if (process.platform === 'darwin') {
                const appPath = path.join(WORKSPACE_ROOT, 'build', 'projects', project.id, 'mac', `${project.title}.app`);
                const macBinary = path.join(appPath, 'Contents', 'MacOS', project.id);
                if (!fs.existsSync(macBinary)) {
                    await buildMac(project, (msg) => console.log(msg));
                }
                if (fs.existsSync(macBinary)) {
                    execCmd = macBinary;
                    execCwd = path.dirname(macBinary);
                    const embeddedWad = path.join(appPath, 'Contents', 'Resources', 'game.wad');
                    if (fs.existsSync(embeddedWad)) {
                        execArgs = ['-iwad', embeddedWad];
                    } else {
                        const wad = resolveWadPath(project.wadPath);
                        if (wad) execArgs = ['-iwad', wad];
                    }
                }
            } else if (process.platform === 'win32') {
                const winDir = path.join(WORKSPACE_ROOT, 'build', 'projects', project.id, 'win');
                const safeTitle = project.title.replace(/[^a-zA-Z0-9_-]/g, '_');
                const winExe = path.join(winDir, `${safeTitle}.exe`);
                if (!fs.existsSync(winExe)) {
                    await buildWin(project, (msg) => console.log(msg));
                }
                if (fs.existsSync(winExe)) {
                    execCmd = winExe;
                    execCwd = winDir;
                    const wad = resolveWadPath(project.wadPath);
                    if (wad) execArgs = ['-iwad', wad];
                }
            } else {
                const linuxDir = path.join(WORKSPACE_ROOT, 'build', 'projects', project.id, 'linux');
                const linuxBin = path.join(linuxDir, project.id);
                if (!fs.existsSync(linuxBin)) {
                    await buildLinux(project, (msg) => console.log(msg));
                }
                if (fs.existsSync(linuxBin)) {
                    execCmd = linuxBin;
                    execCwd = linuxDir;
                    const wad = resolveWadPath(project.wadPath);
                    if (wad) execArgs = ['-iwad', wad];
                }
            }

            if (!execCmd || !fs.existsSync(execCmd)) {
                return sendJson(res, 400, { ok: false, error: 'Could not find or compile game executable for this platform.' });
            }

            activeGameProcess = spawn(execCmd, execArgs, {
                cwd: execCwd,
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

server.on('error', (err) => {
    if (err.code === 'EADDRINUSE') {
        console.log(`[HRGZDevEngine Studio] Port ${PORT} already active, reusing existing instance.`);
    } else {
        console.error(`[HRGZDevEngine Studio] Server error:`, err);
    }
});

server.listen(PORT, '127.0.0.1', () => {
    const url = `http://127.0.0.1:${PORT}`;
    console.log(`=======================================================`);
    console.log(`  HRGZDevEngine Studio - Game Creator & Compiler`);
    console.log(`  Engine Source: ${ENGINE_ROOT}`);
    console.log(`  User Workspace: ${WORKSPACE_ROOT}`);
    console.log(`  Dashboard running at: ${url}`);
    console.log(`=======================================================`);

    // Only auto-open external browser when NOT running inside embedded desktop native wrapper
    if (process.env.HRGZ_EMBEDDED !== '1') {
        const openCommand = process.platform === 'darwin' ? `open "${url}"` :
                            process.platform === 'win32' ? `start "${url}"` :
                            `xdg-open "${url}"`;
        exec(openCommand, () => {});
    }
});
