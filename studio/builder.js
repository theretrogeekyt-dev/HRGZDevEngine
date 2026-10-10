/**
 * HRGZDevEngine Studio - Multi-Platform Game Builder & Distribution Packager
 * Compiles standalone executables for macOS, Windows, and Linux,
 * bundles game assets into native app packages, and creates release distributions (.dmg, .exe, .deb, .zip).
 */

const fs = require('fs');
const path = require('path');
const os = require('os');
const { spawn, execSync } = require('child_process');

function ensureDir(dir) {
    if (!fs.existsSync(dir)) {
        fs.mkdirSync(dir, { recursive: true });
    }
}

/**
 * Discovers the Engine Root containing engine sources (src/doom, src/hal),
 * templates, and default assets.
 */
function getEngineRoot() {
    // 1. Explicit environment variable
    if (process.env.HRGZ_ENGINE_ROOT && fs.existsSync(path.join(process.env.HRGZ_ENGINE_ROOT, 'src', 'doom'))) {
        return process.env.HRGZ_ENGINE_ROOT;
    }
    // 2. Direct parent (git repo root or macOS Contents/Resources)
    const parentDir = path.resolve(__dirname, '..');
    if (fs.existsSync(path.join(parentDir, 'src', 'doom'))) {
        return parentDir;
    }
    // 3. macOS App bundle Resources directory if running from executable
    const macResDir = path.join(__dirname, '..', 'Resources');
    if (fs.existsSync(path.join(macResDir, 'src', 'doom'))) {
        return macResDir;
    }
    // 4. Linux system install
    const linuxShare = '/usr/share/hrgzdevengine-studio';
    if (fs.existsSync(path.join(linuxShare, 'src', 'doom'))) {
        return linuxShare;
    }
    return parentDir;
}

/**
 * Discovers or creates the User Workspace where user projects, custom assets,
 * intermediate builds, and distribution packages reside.
 */
function getWorkspaceRoot() {
    // 1. Explicit environment variable
    if (process.env.HRGZ_WORKSPACE && fs.existsSync(process.env.HRGZ_WORKSPACE)) {
        return process.env.HRGZ_WORKSPACE;
    }
    // 2. If running inside a git development repository, use repo root
    const repoCandidate = path.resolve(__dirname, '..');
    if (fs.existsSync(path.join(repoCandidate, '.git')) && process.env.HRGZ_EMBEDDED !== '1') {
        return repoCandidate;
    }
    // 3. Dedicated user documents workspace
    const userDocs = path.join(os.homedir(), 'Documents');
    const workspace = fs.existsSync(userDocs)
        ? path.join(userDocs, 'HRGZDevEngine')
        : path.join(os.homedir(), 'HRGZDevEngine');

    ensureDir(workspace);
    ensureDir(path.join(workspace, 'build'));
    ensureDir(path.join(workspace, 'dist'));
    return workspace;
}

/**
 * Resolves a WAD file path checking workspace and engine directories.
 */
function resolveWadPath(specifiedPath) {
    const engineRoot = getEngineRoot();
    const workspaceRoot = getWorkspaceRoot();

    if (specifiedPath) {
        if (path.isAbsolute(specifiedPath) && fs.existsSync(specifiedPath)) return specifiedPath;
        const inWorkspace = path.join(workspaceRoot, specifiedPath);
        if (fs.existsSync(inWorkspace)) return inWorkspace;
        const inEngine = path.join(engineRoot, specifiedPath);
        if (fs.existsSync(inEngine)) return inEngine;
    }

    // Default doom1.wad fallback
    const wsDefault = path.join(workspaceRoot, 'doom1.wad');
    if (fs.existsSync(wsDefault)) return wsDefault;

    const engDefault = path.join(engineRoot, 'doom1.wad');
    if (fs.existsSync(engDefault)) return engDefault;

    return null;
}

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
        engineRoot: getEngineRoot(),
        workspaceRoot: getWorkspaceRoot(),
        tools: {
            clang: checkTool('clang'),
            gcc: checkTool('gcc'),
            make: checkTool('make'),
            zip: checkTool('zip'),
            tar: checkTool('tar'),
            mingw: checkTool('x86_64-w64-mingw32-gcc') || checkTool('i686-w64-mingw32-gcc')
        }
    };
}

function runCommand(cmd, args, options, onLog) {
    return new Promise((resolve, reject) => {
        onLog(`> ${cmd} ${args.join(' ')}\n`);
        const proc = spawn(cmd, args, { cwd: options.cwd || getWorkspaceRoot(), env: process.env });

        proc.stdout.on('data', (d) => onLog(d.toString()));
        proc.stderr.on('data', (d) => onLog(d.toString()));

        proc.on('close', (code) => {
            if (code === 0) resolve();
            else reject(new Error(`Command exited with status code ${code}`));
        });
        proc.on('error', (err) => reject(err));
    });
}

async function buildMac(project, onLog) {
    const engineRoot = getEngineRoot();
    const workspaceRoot = getWorkspaceRoot();

    onLog(`[BUILD] Starting native macOS compilation for '${project.title}'...\n`);
    onLog(`[ENGINE] Engine Source Root: ${engineRoot}\n`);
    onLog(`[WORKSPACE] Project Workspace: ${workspaceRoot}\n`);

    const buildDir = path.join(workspaceRoot, 'build', 'projects', project.id, 'mac');
    ensureDir(buildDir);

    const appDir = path.join(buildDir, `${project.title}.app`);
    const macosDir = path.join(appDir, 'Contents', 'MacOS');
    const resDir = path.join(appDir, 'Contents', 'Resources');
    ensureDir(macosDir);
    ensureDir(resDir);

    const binaryPath = path.join(macosDir, project.id);
    const doomDir = path.join(engineRoot, 'src', 'doom');
    let compiled = false;

    // 1. Compile from engine sources if clang and sources are available
    if (fs.existsSync(doomDir) && checkTool('clang')) {
        onLog(`[BUILD] Compiling native Metal/Cocoa executable using host clang...\n`);
        const doomSources = fs.readdirSync(doomDir)
            .filter(f => f.endsWith('.c'))
            .map(f => path.join(doomDir, f));

        const commonDir = path.join(engineRoot, 'src', 'hal', 'common');
        const commonSources = fs.existsSync(commonDir)
            ? fs.readdirSync(commonDir).filter(f => f.endsWith('.c')).map(f => path.join(commonDir, f))
            : [];

        const macDir = path.join(engineRoot, 'src', 'hal', 'mac');
        const macSources = fs.existsSync(macDir)
            ? fs.readdirSync(macDir).filter(f => f.endsWith('.c') || f.endsWith('.m')).map(f => path.join(macDir, f))
            : [];

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
            `-I${path.join(engineRoot, 'src', 'doom')}`,
            `-I${path.join(engineRoot, 'src', 'hal', 'common')}`,
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

        try {
            await runCommand('clang', cflags, { cwd: workspaceRoot }, onLog);
            onLog(`[BUILD] Native executable compiled successfully: ${binaryPath}\n`);
            compiled = true;
        } catch (err) {
            onLog(`[WARN] Native clang compilation failed: ${err.message}. Checking pre-built engine templates...\n`);
        }
    }

    // 2. Fall back to bundled pre-compiled template if not compiled from source
    if (!compiled) {
        const candidateTemplates = [
            path.join(engineRoot, 'templates', 'mac', 'DOOM.app', 'Contents', 'MacOS', 'DOOM'),
            path.join(engineRoot, 'templates', 'mac', 'doom_mac'),
            path.join(engineRoot, 'build', 'mac', 'doom_mac'),
            path.join(engineRoot, 'build', 'mac', 'DOOM.app', 'Contents', 'MacOS', 'DOOM'),
            path.join(workspaceRoot, 'build', 'mac', 'doom_mac')
        ];
        let foundTemplate = null;
        for (const cand of candidateTemplates) {
            if (fs.existsSync(cand)) {
                foundTemplate = cand;
                break;
            }
        }
        if (foundTemplate) {
            onLog(`[BUILD] Injecting bundled pre-compiled macOS engine runtime: ${foundTemplate}\n`);
            fs.copyFileSync(foundTemplate, binaryPath);
            fs.chmodSync(binaryPath, 0o755);
            compiled = true;
        } else {
            throw new Error(`Engine sources not found at ${doomDir} and no pre-built engine templates found.`);
        }
    }

    // Bundle game WAD
    const wadSrc = resolveWadPath(project.wadPath);
    if (wadSrc && fs.existsSync(wadSrc)) {
        const destWad = path.join(resDir, 'game.wad');
        fs.copyFileSync(wadSrc, destWad);
        onLog(`[ASSETS] Bundled game WAD copied to ${destWad} (${(fs.statSync(destWad).size / (1024 * 1024)).toFixed(2)} MB)\n`);
    } else {
        onLog(`[WARN] No WAD file located. Standalone game bundle may require IWAD file.\n`);
    }

    // Write game manifest
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

    // Clean and ad-hoc code sign bundle
    onLog(`[SIGN] Ad-hoc signing application bundle...\n`);
    try {
        try { await runCommand('dot_clean', ['-m', appDir], { cwd: workspaceRoot }, () => {}); } catch {}
        await runCommand('xattr', ['-cr', appDir], { cwd: workspaceRoot }, onLog);
        await runCommand('codesign', ['--force', '--deep', '--sign', '-', appDir], { cwd: workspaceRoot }, onLog);
    } catch (e) {
        onLog(`[WARN] Codesign returned: ${e.message}\n`);
    }

    onLog(`[SUCCESS] Standalone macOS bundle ready: ${appDir}\n`);
    return appDir;
}

async function buildWin(project, onLog) {
    const engineRoot = getEngineRoot();
    const workspaceRoot = getWorkspaceRoot();

    onLog(`[BUILD] Starting native Windows compilation for '${project.title}'...\n`);
    const buildDir = path.join(workspaceRoot, 'build', 'projects', project.id, 'win');
    ensureDir(buildDir);

    const safeTitle = project.title.replace(/[^a-zA-Z0-9_-]/g, '_');
    const exePath = path.join(buildDir, `${safeTitle}.exe`);

    let compiler = null;
    for (const c of ['x86_64-w64-mingw32-gcc', 'i686-w64-mingw32-gcc']) {
        if (checkTool(c)) { compiler = c; break; }
    }
    if (!compiler && process.platform === 'win32' && checkTool('gcc')) {
        compiler = 'gcc';
    }

    const doomDir = path.join(engineRoot, 'src', 'doom');
    let compiled = false;

    if (compiler && fs.existsSync(doomDir)) {
        onLog(`[BUILD] Using Windows compiler: ${compiler}\n`);
        const doomSources = fs.readdirSync(doomDir)
            .filter(f => f.endsWith('.c'))
            .map(f => path.join(doomDir, f));

        const commonDir = path.join(engineRoot, 'src', 'hal', 'common');
        const commonSources = fs.existsSync(commonDir)
            ? fs.readdirSync(commonDir).filter(f => f.endsWith('.c')).map(f => path.join(commonDir, f))
            : [];

        const winDir = path.join(engineRoot, 'src', 'hal', 'win32');
        const winSources = fs.existsSync(winDir)
            ? fs.readdirSync(winDir).filter(f => f.endsWith('.c')).map(f => path.join(winDir, f))
            : [];

        const allSources = [...doomSources, ...commonSources, ...winSources];

        const cflags = [
            '-O3',
            '-fomit-frame-pointer',
            '-Wall',
            '-Wno-parentheses',
            '-Wno-unused-const-variable',
            '-Wno-unused-but-set-variable',
            '-Wno-unused-variable',
            '-std=c99',
            `-DHRGZ_GAME_TITLE="${project.title.replace(/"/g, '\\"')}"`,
            `-DHRGZ_GAME_ID="${project.id.replace(/"/g, '\\"')}"`,
            `-I${path.join(engineRoot, 'src', 'doom')}`,
            `-I${path.join(engineRoot, 'src', 'hal', 'common')}`,
            ...allSources,
            '-lgdi32', '-lwinmm', '-lws2_32', '-lopengl32', '-lm', '-s',
            '-o', exePath
        ];

        try {
            await runCommand(compiler, cflags, { cwd: workspaceRoot }, onLog);
            onLog(`[BUILD] Windows executable created: ${exePath}\n`);
            compiled = true;
        } catch (err) {
            onLog(`[WARN] MinGW build failed: ${err.message}. Checking pre-built template...\n`);
        }
    }

    if (!compiled) {
        const candidateTemplates = [
            path.join(engineRoot, 'templates', 'win', 'doom.exe'),
            path.join(engineRoot, 'build', 'win', 'doom.exe'),
            path.join(workspaceRoot, 'build', 'win', 'doom.exe')
        ];
        let foundTemplate = null;
        for (const cand of candidateTemplates) {
            if (fs.existsSync(cand)) { foundTemplate = cand; break; }
        }
        if (foundTemplate) {
            onLog(`[BUILD] Using pre-built Windows engine template: ${foundTemplate}\n`);
            fs.copyFileSync(foundTemplate, exePath);
            compiled = true;
        } else {
            onLog(`[WARN] MinGW compiler not detected and no pre-built doom.exe found.\n`);
        }
    }

    return exePath;
}

async function buildLinux(project, onLog) {
    const engineRoot = getEngineRoot();
    const workspaceRoot = getWorkspaceRoot();

    onLog(`[BUILD] Starting native Linux compilation for '${project.title}'...\n`);
    const buildDir = path.join(workspaceRoot, 'build', 'projects', project.id, 'linux');
    ensureDir(buildDir);

    const binPath = path.join(buildDir, project.id);
    let compiler = checkTool('gcc') ? 'gcc' : (checkTool('clang') ? 'clang' : null);
    const doomDir = path.join(engineRoot, 'src', 'doom');
    let compiled = false;

    if (compiler && fs.existsSync(doomDir)) {
        onLog(`[BUILD] Using Linux compiler: ${compiler}\n`);
        const doomSources = fs.readdirSync(doomDir)
            .filter(f => f.endsWith('.c'))
            .map(f => path.join(doomDir, f));

        const commonDir = path.join(engineRoot, 'src', 'hal', 'common');
        const commonSources = fs.existsSync(commonDir)
            ? fs.readdirSync(commonDir).filter(f => f.endsWith('.c')).map(f => path.join(commonDir, f))
            : [];

        const sdlDir = path.join(engineRoot, 'src', 'hal', 'sdl');
        const sdlSources = fs.existsSync(sdlDir)
            ? fs.readdirSync(sdlDir).filter(f => f.endsWith('.c')).map(f => path.join(sdlDir, f))
            : [];

        const allSources = [...doomSources, ...commonSources, ...sdlSources];

        const cflags = [
            '-O3',
            '-fomit-frame-pointer',
            '-Wall',
            '-Wno-parentheses',
            '-Wno-unused-const-variable',
            '-Wno-unused-but-set-variable',
            '-Wno-unused-variable',
            '-std=c99',
            `-DHRGZ_GAME_TITLE="${project.title.replace(/"/g, '\\"')}"`,
            `-DHRGZ_GAME_ID="${project.id.replace(/"/g, '\\"')}"`,
            `-I${path.join(engineRoot, 'src', 'doom')}`,
            `-I${path.join(engineRoot, 'src', 'hal', 'common')}`,
            ...allSources,
            '-lSDL2', '-lm', '-s',
            '-o', binPath
        ];

        try {
            await runCommand(compiler, cflags, { cwd: workspaceRoot }, onLog);
            onLog(`[BUILD] Linux executable created: ${binPath}\n`);
            compiled = true;
        } catch (e) {
            onLog(`[WARN] Linux build with SDL2 skipped or failed on host: ${e.message}\n`);
        }
    }

    if (!compiled) {
        const candidateTemplates = [
            path.join(engineRoot, 'templates', 'linux', 'doom_sdl'),
            path.join(engineRoot, 'build', 'linux', 'doom_sdl'),
            path.join(workspaceRoot, 'build', 'linux', 'doom_sdl')
        ];
        let foundTemplate = null;
        for (const cand of candidateTemplates) {
            if (fs.existsSync(cand)) { foundTemplate = cand; break; }
        }
        if (foundTemplate) {
            onLog(`[BUILD] Using pre-built Linux engine template: ${foundTemplate}\n`);
            fs.copyFileSync(foundTemplate, binPath);
            fs.chmodSync(binPath, 0o755);
            compiled = true;
        }
    }

    return binPath;
}

async function packageRelease(project, target, onLog) {
    const workspaceRoot = getWorkspaceRoot();
    const distDir = path.join(workspaceRoot, 'dist', project.id);
    ensureDir(distDir);

    const safeTitle = project.title.replace(/[^a-zA-Z0-9_-]/g, '_');
    const version = project.version || '1.0.0';

    if (target === 'mac' || target === 'dmg') {
        let appPath = path.join(workspaceRoot, 'build', 'projects', project.id, 'mac', `${project.title}.app`);
        if (!fs.existsSync(appPath)) {
            onLog(`[DIST] App bundle not found, compiling '${project.title}' first...\n`);
            appPath = await buildMac(project, onLog);
        }

        // 1. Build .dmg (Apple Disk Image)
        const dmgStaging = path.join(workspaceRoot, 'build', 'projects', project.id, 'dmg_staging');
        if (fs.existsSync(dmgStaging)) fs.rmSync(dmgStaging, { recursive: true, force: true });
        ensureDir(dmgStaging);

        fs.cpSync(appPath, path.join(dmgStaging, `${project.title}.app`), { recursive: true });
        try {
            fs.symlinkSync('/Applications', path.join(dmgStaging, 'Applications'));
        } catch {}

        const dmgName = `${safeTitle}-v${version}-macOS.dmg`;
        const dmgPath = path.join(distDir, dmgName);
        if (fs.existsSync(dmgPath)) fs.unlinkSync(dmgPath);

        onLog(`[DIST] Creating macOS Apple Disk Image (.dmg) at ${dmgPath}...\n`);
        await runCommand('hdiutil', ['create', '-volname', project.title, '-srcfolder', dmgStaging, '-ov', '-format', 'UDZO', dmgPath], { cwd: workspaceRoot }, onLog);

        // 2. Also generate .zip for itch.io / Steam
        const zipName = `${safeTitle}-v${version}-macOS.zip`;
        const zipPath = path.join(distDir, zipName);
        if (fs.existsSync(zipPath)) fs.unlinkSync(zipPath);
        const macBuildDir = path.dirname(appPath);
        await runCommand('zip', ['-r', '-y', '-X', zipPath, `${project.title}.app`], { cwd: macBuildDir }, onLog);

        const itchToml = `[[actions]]\nname = "play"\npath = "${project.title}.app"\n`;
        fs.writeFileSync(path.join(distDir, 'itch.toml'), itchToml);

        onLog(`[DIST] macOS .dmg created: ${dmgPath} (${(fs.statSync(dmgPath).size / (1024 * 1024)).toFixed(2)} MB)\n`);
        return dmgPath;
    }

    if (target === 'linux' || target === 'deb') {
        onLog(`[DIST] Creating Debian Linux package (.deb) for '${project.title}'...\n`);
        let linuxBin = path.join(workspaceRoot, 'build', 'projects', project.id, 'linux', project.id);
        if (!fs.existsSync(linuxBin)) {
            linuxBin = await buildLinux(project, onLog);
        }

        const debStaging = path.join(workspaceRoot, 'build', 'projects', project.id, 'deb_staging');
        if (fs.existsSync(debStaging)) fs.rmSync(debStaging, { recursive: true, force: true });
        ensureDir(debStaging);

        const controlDir = path.join(debStaging, 'control_dir');
        const dataDir = path.join(debStaging, 'data_dir');
        ensureDir(controlDir);
        ensureDir(dataDir);

        const control = `Package: ${project.id}
Version: ${version}
Section: games
Priority: optional
Architecture: all
Maintainer: ${project.author || 'HRGZDevEngine'}
Description: ${project.description || project.title}
`;
        fs.writeFileSync(path.join(controlDir, 'control'), control);

        const usrGames = path.join(dataDir, 'usr', 'games');
        const usrShare = path.join(dataDir, 'usr', 'share', 'games', project.id);
        const usrApps = path.join(dataDir, 'usr', 'share', 'applications');
        ensureDir(usrGames);
        ensureDir(usrShare);
        ensureDir(usrApps);

        if (fs.existsSync(linuxBin)) {
            fs.copyFileSync(linuxBin, path.join(usrGames, project.id));
            fs.chmodSync(path.join(usrGames, project.id), 0o755);
        }

        const wadSrc = resolveWadPath(project.wadPath);
        if (wadSrc && fs.existsSync(wadSrc)) {
            fs.copyFileSync(wadSrc, path.join(usrShare, 'game.wad'));
        }

        const runner = `#!/bin/sh\nexec /usr/games/${project.id} -iwad /usr/share/games/${project.id}/game.wad "$@"\n`;
        fs.writeFileSync(path.join(usrGames, `${project.id}-launcher`), runner);
        fs.chmodSync(path.join(usrGames, `${project.id}-launcher`), 0o755);

        const desktop = `[Desktop Entry]\nName=${project.title}\nExec=/usr/games/${project.id}-launcher\nType=Application\nCategories=Game;\n`;
        fs.writeFileSync(path.join(usrApps, `${project.id}.desktop`), desktop);

        const debianBinary = path.join(debStaging, 'debian-binary');
        fs.writeFileSync(debianBinary, '2.0\n');
        const controlTar = path.join(debStaging, 'control.tar.gz');
        const dataTar = path.join(debStaging, 'data.tar.gz');

        await runCommand('tar', ['-czf', controlTar, '-C', controlDir, '.'], { cwd: debStaging }, onLog);
        await runCommand('tar', ['-czf', dataTar, '-C', dataDir, '.'], { cwd: debStaging }, onLog);

        const debOutput = path.join(distDir, `${project.id}_${version}_all.deb`);
        await runCommand('ar', ['-q', '-S', debOutput, 'debian-binary', 'control.tar.gz', 'data.tar.gz'], { cwd: debStaging }, onLog);

        onLog(`[DIST] Linux .deb package created: ${debOutput} (${(fs.statSync(debOutput).size / (1024 * 1024)).toFixed(2)} MB)\n`);
        return debOutput;
    }

    if (target === 'win' || target === 'exe') {
        onLog(`[DIST] Creating Windows standalone distribution for '${project.title}'...\n`);
        const winDir = path.join(distDir, `${safeTitle}-Windows`);
        if (fs.existsSync(winDir)) fs.rmSync(winDir, { recursive: true, force: true });
        ensureDir(winDir);

        let exePath = path.join(workspaceRoot, 'build', 'projects', project.id, 'win', `${safeTitle}.exe`);
        if (!fs.existsSync(exePath)) {
            exePath = await buildWin(project, onLog);
        }

        if (fs.existsSync(exePath)) {
            fs.copyFileSync(exePath, path.join(winDir, `${safeTitle}.exe`));
        }

        const wadSrc = resolveWadPath(project.wadPath);
        if (wadSrc && fs.existsSync(wadSrc)) {
            fs.copyFileSync(wadSrc, path.join(winDir, 'game.wad'));
        }
        fs.writeFileSync(path.join(winDir, 'game.json'), JSON.stringify(project, null, 2));

        // Quick launcher batch script
        const launcherBat = `@echo off\r\nstart "" "%~dp0${safeTitle}.exe" -iwad "%~dp0game.wad" %*\r\n`;
        fs.writeFileSync(path.join(winDir, `Launch_${safeTitle}.bat`), launcherBat);

        const itchToml = `[[actions]]\nname = "play"\npath = "${safeTitle}.exe"\n`;
        fs.writeFileSync(path.join(distDir, 'itch.toml'), itchToml);

        const zipName = `${safeTitle}-v${version}-Windows.zip`;
        const zipPath = path.join(distDir, zipName);
        if (fs.existsSync(zipPath)) fs.unlinkSync(zipPath);

        await runCommand('zip', ['-r', '-y', zipPath, path.basename(winDir)], { cwd: distDir }, onLog);
        onLog(`[DIST] Windows standalone package created: ${zipPath} (${(fs.statSync(zipPath).size / (1024 * 1024)).toFixed(2)} MB)\n`);
        return zipPath;
    }

    if (target === 'all') {
        onLog(`[DIST] Generating all release distribution packages for '${project.title}'...\n`);
        const results = [];
        results.push(await packageRelease(project, 'dmg', onLog));
        results.push(await packageRelease(project, 'exe', onLog));
        results.push(await packageRelease(project, 'deb', onLog));
        onLog(`\n[COMPLETE] All platform distribution packages generated in: ${distDir}\n`);
        return results[0];
    }

    throw new Error(`Unsupported package target: ${target}`);
}

module.exports = {
    getEngineRoot,
    getWorkspaceRoot,
    resolveWadPath,
    detectSystem,
    buildMac,
    buildWin,
    buildLinux,
    packageRelease
};
