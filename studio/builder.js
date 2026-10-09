/**
 * HRGZDevEngine DOOM Multi-Platform Builder & Distribution Packager
 * Compiles standalone executables for macOS, Windows, and Linux,
 * bundles game assets into native app packages, and creates release zips.
 */

const fs = require('fs');
const path = require('path');
const { spawn, execSync } = require('child_process');

const REPO_ROOT = path.resolve(__dirname, '..');

function checkTool(cmd) {
    try {
        execSync(`which ${cmd} 2>/dev/null`, { stdio: 'ignore' });
        return true;
    } catch {
        return false;
    }
}

function detectSystem() {
    return {
        platform: process.platform,
        arch: process.arch,
        tools: {
            clang: checkTool('clang'),
            gcc: checkTool('gcc'),
            make: checkTool('make'),
            zip: checkTool('zip'),
            tar: checkTool('tar'),
            mingw: checkTool('x86_64-w64-mingw32-gcc')
        }
    };
}

function runCommand(cmd, args, options, onLog) {
    return new Promise((resolve, reject) => {
        onLog(`> ${cmd} ${args.join(' ')}\n`);
        const proc = spawn(cmd, args, { cwd: options.cwd || REPO_ROOT, env: process.env });

        proc.stdout.on('data', (d) => onLog(d.toString()));
        proc.stderr.on('data', (d) => onLog(d.toString()));

        proc.on('close', (code) => {
            if (code === 0) resolve();
            else reject(new Error(`Command exited with status code ${code}`));
        });
        proc.on('error', (err) => reject(err));
    });
}

function ensureDir(dir) {
    if (!fs.existsSync(dir)) {
        fs.mkdirSync(dir, { recursive: true });
    }
}

async function buildMac(project, onLog) {
    onLog(`[BUILD] Starting native macOS compilation for '${project.title}'...\n`);
    const buildDir = path.join(REPO_ROOT, 'build', 'projects', project.id, 'mac');
    ensureDir(buildDir);

    const appDir = path.join(buildDir, `${project.title}.app`);
    const macosDir = path.join(appDir, 'Contents', 'MacOS');
    const resDir = path.join(appDir, 'Contents', 'Resources');
    ensureDir(macosDir);
    ensureDir(resDir);

    const binaryPath = path.join(macosDir, project.id);

    // Source files
    const doomSources = fs.readdirSync(path.join(REPO_ROOT, 'src', 'doom'))
        .filter(f => f.endsWith('.c'))
        .map(f => path.join('src', 'doom', f));

    const commonSources = fs.readdirSync(path.join(REPO_ROOT, 'src', 'hal', 'common'))
        .filter(f => f.endsWith('.c'))
        .map(f => path.join('src', 'hal', 'common', f));

    const macSources = fs.readdirSync(path.join(REPO_ROOT, 'src', 'hal', 'mac'))
        .filter(f => f.endsWith('.c') || f.endsWith('.m'))
        .map(f => path.join('src', 'hal', 'mac', f));

    const allSources = [...doomSources, ...commonSources, ...macSources];

    const cflags = [
        '-O3',
        '-fomit-frame-pointer',
        '-Wall',
        '-Wno-parentheses',
        '-Wno-unused-const-variable',
        '-Wno-unused-but-set-variable',
        '-Wno-unused-variable',
        '-Wno-unknown-warning-option',
        '-std=c99',
        `-DHRGZ_GAME_TITLE="${project.title.replace(/"/g, '\\"')}"`,
        `-DHRGZ_GAME_ID="${project.id.replace(/"/g, '\\"')}"`,
        '-Isrc/doom',
        '-Isrc/hal/common',
        ...allSources,
        '-framework', 'Cocoa',
        '-framework', 'Metal',
        '-framework', 'QuartzCore',
        '-framework', 'GameController',
        '-framework', 'AudioToolbox',
        '-framework', 'CoreFoundation',
        '-framework', 'Carbon',
        '-lm',
        '-o', binaryPath
    ];

    await runCommand('clang', cflags, { cwd: REPO_ROOT }, onLog);
    onLog(`[BUILD] Compiled binary: ${binaryPath}\n`);

    // Copy bundled game WAD
    const wadSrc = project.wadPath ? path.resolve(REPO_ROOT, project.wadPath) : path.join(REPO_ROOT, 'doom1.wad');
    if (fs.existsSync(wadSrc)) {
        const destWad = path.join(resDir, 'game.wad');
        fs.copyFileSync(wadSrc, destWad);
        onLog(`[ASSETS] Bundled game WAD copied to ${destWad} (${(fs.statSync(destWad).size / (1024*1024)).toFixed(2)} MB)\n`);
    }

    // Write game.json manifest
    fs.writeFileSync(path.join(resDir, 'game.json'), JSON.stringify(project, null, 2));

    // Generate Info.plist
    const plist = `<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>en</string>
    <key>CFBundleExecutable</key>
    <string>${project.id}</string>
    <key>CFBundleIdentifier</key>
    <string>com.${project.author ? project.author.toLowerCase().replace(/[^a-z0-9]/g, '') : 'hrgz'}.${project.id}</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>${project.title}</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>${project.version || '1.0.0'}</string>
    <key>CFBundleVersion</key>
    <string>1</string>
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>`;
    fs.writeFileSync(path.join(appDir, 'Contents', 'Info.plist'), plist);

    // Ad-hoc code sign
    onLog(`[SIGN] Ad-hoc signing application bundle...\n`);
    try {
        await runCommand('xattr', ['-cr', appDir], { cwd: REPO_ROOT }, onLog);
        await runCommand('codesign', ['--force', '--deep', '--sign', '-', appDir], { cwd: REPO_ROOT }, onLog);
    } catch (e) {
        onLog(`[WARN] Ad-hoc codesign skipped or returned: ${e.message}\n`);
    }

    onLog(`[SUCCESS] macOS native bundle complete: ${appDir}\n`);
    return appDir;
}

async function packageRelease(project, target, onLog) {
    const distDir = path.join(REPO_ROOT, 'dist', project.id);
    ensureDir(distDir);

    const safeTitle = project.title.replace(/[^a-zA-Z0-9_-]/g, '_');
    const version = project.version || '1.0.0';

    if (target === 'mac') {
        const appPath = path.join(REPO_ROOT, 'build', 'projects', project.id, 'mac', `${project.title}.app`);
        if (!fs.existsSync(appPath)) {
            throw new Error(`macOS app bundle not found at ${appPath}. Build macOS first!`);
        }

        const zipName = `${safeTitle}-v${version}-macOS.zip`;
        const zipPath = path.join(distDir, zipName);
        if (fs.existsSync(zipPath)) fs.unlinkSync(zipPath);

        const macBuildDir = path.dirname(appPath);
        onLog(`[DIST] Zipping macOS application into ${zipPath}...\n`);
        await runCommand('zip', ['-r', '-y', '-X', zipPath, `${project.title}.app`], { cwd: macBuildDir }, onLog);

        // Generate itch.io Butler configuration
        const itchToml = `[[actions]]
name = "play"
path = "${project.title}.app"
`;
        fs.writeFileSync(path.join(distDir, 'itch.toml'), itchToml);

        onLog(`[DIST] Package created: ${zipPath} (${(fs.statSync(zipPath).size / (1024*1024)).toFixed(2)} MB)\n`);
        return zipPath;
    }

    throw new Error(`Unsupported package target: ${target}`);
}

module.exports = {
    detectSystem,
    buildMac,
    packageRelease
};
