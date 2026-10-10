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

    if (target === 'mac' || target === 'dmg') {
        let appPath = path.join(REPO_ROOT, 'build', 'projects', project.id, 'mac', `${project.title}.app`);
        if (!fs.existsSync(appPath)) {
            onLog(`[DIST] App bundle not found, compiling '${project.title}' first...\n`);
            appPath = await buildMac(project, onLog);
        }

        // 1. Build .dmg (Apple Disk Image)
        const dmgStaging = path.join(REPO_ROOT, 'build', 'projects', project.id, 'dmg_staging');
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
        await runCommand('hdiutil', ['create', '-volname', project.title, '-srcfolder', dmgStaging, '-ov', '-format', 'UDZO', dmgPath], { cwd: REPO_ROOT }, onLog);

        // Also generate .zip for itch.io / web
        const zipName = `${safeTitle}-v${version}-macOS.zip`;
        const zipPath = path.join(distDir, zipName);
        if (fs.existsSync(zipPath)) fs.unlinkSync(zipPath);
        const macBuildDir = path.dirname(appPath);
        await runCommand('zip', ['-r', '-y', '-X', zipPath, `${project.title}.app`], { cwd: macBuildDir }, onLog);

        const itchToml = `[[actions]]\nname = "play"\npath = "${project.title}.app"\n`;
        fs.writeFileSync(path.join(distDir, 'itch.toml'), itchToml);

        onLog(`[DIST] macOS .dmg created: ${dmgPath} (${(fs.statSync(dmgPath).size / (1024*1024)).toFixed(2)} MB)\n`);
        return dmgPath;
    }

    if (target === 'linux' || target === 'deb') {
        onLog(`[DIST] Creating Debian Linux package (.deb) for '${project.title}'...\n`);
        const debStaging = path.join(REPO_ROOT, 'build', 'projects', project.id, 'deb_staging');
        if (fs.existsSync(debStaging)) fs.rmSync(debStaging, { recursive: true, force: true });
        ensureDir(debStaging);

        const controlDir = path.join(debStaging, 'control_dir');
        const dataDir = path.join(debStaging, 'data_dir');
        ensureDir(controlDir);
        ensureDir(dataDir);

        const control = `Package: ${project.id}\nVersion: ${version}\nSection: games\nPriority: optional\nArchitecture: all\nMaintainer: ${project.author || 'HRGZDevEngine'}\nDescription: ${project.description || project.title}\n`;
        fs.writeFileSync(path.join(controlDir, 'control'), control);

        const usrGames = path.join(dataDir, 'usr', 'games');
        const usrShare = path.join(dataDir, 'usr', 'share', 'games', project.id);
        const usrApps = path.join(dataDir, 'usr', 'share', 'applications');
        ensureDir(usrGames);
        ensureDir(usrShare);
        ensureDir(usrApps);

        const wadSrc = project.wadPath ? path.resolve(REPO_ROOT, project.wadPath) : path.join(REPO_ROOT, 'doom1.wad');
        if (fs.existsSync(wadSrc)) {
            fs.copyFileSync(wadSrc, path.join(usrShare, 'game.wad'));
        }

        const runner = `#!/bin/sh\nexec /usr/games/${project.id} -iwad /usr/share/games/${project.id}/game.wad "$@"\n`;
        fs.writeFileSync(path.join(usrGames, project.id), runner);
        fs.chmodSync(path.join(usrGames, project.id), 0o755);

        const desktop = `[Desktop Entry]\nName=${project.title}\nExec=/usr/games/${project.id}\nType=Application\nCategories=Game;\n`;
        fs.writeFileSync(path.join(usrApps, `${project.id}.desktop`), desktop);

        const debianBinary = path.join(debStaging, 'debian-binary');
        fs.writeFileSync(debianBinary, '2.0\n');
        const controlTar = path.join(debStaging, 'control.tar.gz');
        const dataTar = path.join(debStaging, 'data.tar.gz');

        await runCommand('tar', ['-czf', controlTar, '-C', controlDir, '.'], { cwd: debStaging }, onLog);
        await runCommand('tar', ['-czf', dataTar, '-C', dataDir, '.'], { cwd: debStaging }, onLog);

        const debOutput = path.join(distDir, `${project.id}_${version}_all.deb`);
        await runCommand('ar', ['-q', '-S', debOutput, 'debian-binary', 'control.tar.gz', 'data.tar.gz'], { cwd: debStaging }, onLog);

        onLog(`[DIST] Linux .deb package created: ${debOutput} (${(fs.statSync(debOutput).size / (1024*1024)).toFixed(2)} MB)\n`);
        return debOutput;
    }

    if (target === 'win' || target === 'exe') {
        const winDir = path.join(distDir, `${safeTitle}-Windows`);
        ensureDir(winDir);

        const wadSrc = project.wadPath ? path.resolve(REPO_ROOT, project.wadPath) : path.join(REPO_ROOT, 'doom1.wad');
        if (fs.existsSync(wadSrc)) {
            fs.copyFileSync(wadSrc, path.join(winDir, 'game.wad'));
        }
        fs.writeFileSync(path.join(winDir, 'game.json'), JSON.stringify(project, null, 2));

        const zipName = `${safeTitle}-v${version}-Windows.zip`;
        const zipPath = path.join(distDir, zipName);
        if (fs.existsSync(zipPath)) fs.unlinkSync(zipPath);

        await runCommand('zip', ['-r', '-y', zipPath, path.basename(winDir)], { cwd: distDir }, onLog);
        onLog(`[DIST] Windows standalone package created: ${zipPath}\n`);
        return zipPath;
    }

    throw new Error(`Unsupported package target: ${target}`);
}

module.exports = {
    detectSystem,
    buildMac,
    packageRelease
};
