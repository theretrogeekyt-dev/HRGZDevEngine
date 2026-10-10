/**
 * HRGZDevEngine Studio - Universal Desktop Packaging Engine
 * Builds:
 *  - macOS: Dedicated Apple Disk Image (.dmg) containing native Cocoa/WebKit app
 *  - Linux: Dedicated Debian Package (.deb) with desktop entry and CLI
 *  - Windows: Dedicated .exe standalone launcher and distribution bundle
 */

const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

const REPO_ROOT = path.resolve(__dirname, '..');
const DIST_DIR = path.join(REPO_ROOT, 'dist');
const BUILD_DIR = path.join(REPO_ROOT, 'build', 'studio');

function ensureDir(dir) {
    if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true });
}

function run(cmd, cwd = REPO_ROOT) {
    console.log(`> ${cmd}`);
    execSync(cmd, { cwd, stdio: 'inherit' });
}

function packageMacDMG() {
    console.log('\n=======================================================');
    console.log(' [macOS] Building HRGZDevEngine Studio Native .dmg');
    console.log('=======================================================');

    ensureDir(DIST_DIR);
    ensureDir(BUILD_DIR);

    const appDir = path.join(BUILD_DIR, 'HRGZDevEngine Studio.app');
    const macosDir = path.join(appDir, 'Contents', 'MacOS');
    const resDir = path.join(appDir, 'Contents', 'Resources');

    if (fs.existsSync(appDir)) {
        fs.rmSync(appDir, { recursive: true, force: true });
    }
    ensureDir(macosDir);
    ensureDir(resDir);

    // 1. Compile native Cocoa + WebKit wrapper
    const binaryPath = path.join(macosDir, 'HRGZDevEngine Studio');
    const srcFile = path.join(REPO_ROOT, 'studio', 'native', 'mac', 'main.m');

    run(`clang -O3 -fobjc-arc -framework Cocoa -framework WebKit "${srcFile}" -o "${binaryPath}"`);

    // 2. Bundle studio files into Contents/Resources/studio
    const destStudio = path.join(resDir, 'studio');
    ensureDir(destStudio);
    fs.cpSync(path.join(REPO_ROOT, 'studio'), destStudio, { recursive: true });

    // Also copy bin/hrgz into Resources
    const destBin = path.join(resDir, 'bin');
    ensureDir(destBin);
    fs.cpSync(path.join(REPO_ROOT, 'bin'), destBin, { recursive: true });

    // 3. Generate Info.plist
    const plist = `<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>en</string>
    <key>CFBundleExecutable</key>
    <string>HRGZDevEngine Studio</string>
    <key>CFBundleIdentifier</key>
    <string>com.hrgzdevengine.studio</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>HRGZDevEngine Studio</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>1.0.0</string>
    <key>CFBundleVersion</key>
    <string>1</string>
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>`;
    fs.writeFileSync(path.join(appDir, 'Contents', 'Info.plist'), plist);

    // 4. Code sign bundle
    run(`find "${appDir}" -name ".DS_Store" -delete 2>/dev/null || true`);
    run(`xattr -cr "${appDir}" 2>/dev/null || true`);
    try {
        run(`codesign --force --deep --sign - "${appDir}"`);
    } catch (e) {
        console.log(`[WARN] Codesign returned: ${e.message}`);
    }

    // 5. Create DMG staging folder with Applications shortcut
    const dmgStaging = path.join(BUILD_DIR, 'dmg_staging');
    if (fs.existsSync(dmgStaging)) fs.rmSync(dmgStaging, { recursive: true, force: true });
    ensureDir(dmgStaging);

    // Copy .app to staging
    fs.cpSync(appDir, path.join(dmgStaging, 'HRGZDevEngine Studio.app'), { recursive: true });

    // Create /Applications symlink
    try {
        fs.symlinkSync('/Applications', path.join(dmgStaging, 'Applications'));
    } catch {}

    // 6. Generate DMG using hdiutil
    const dmgOutput = path.join(DIST_DIR, 'HRGZDevEngine-Studio-macOS.dmg');
    if (fs.existsSync(dmgOutput)) fs.unlinkSync(dmgOutput);

    run(`hdiutil create -volname "HRGZDevEngine Studio" -srcfolder "${dmgStaging}" -ov -format UDZO "${dmgOutput}"`);

    console.log(`[SUCCESS] macOS Disk Image (.dmg) created: ${dmgOutput} (${(fs.statSync(dmgOutput).size / (1024*1024)).toFixed(2)} MB)\n`);
    return dmgOutput;
}

function packageLinuxDeb() {
    console.log('\n=======================================================');
    console.log(' [Linux] Building HRGZDevEngine Studio Debian Package (.deb)');
    console.log('=======================================================');

    ensureDir(DIST_DIR);
    ensureDir(BUILD_DIR);

    const debStaging = path.join(BUILD_DIR, 'deb_staging');
    if (fs.existsSync(debStaging)) fs.rmSync(debStaging, { recursive: true, force: true });
    ensureDir(debStaging);

    const controlDir = path.join(debStaging, 'control_dir');
    const dataDir = path.join(debStaging, 'data_dir');
    ensureDir(controlDir);
    ensureDir(dataDir);

    // 1. Control file
    const control = `Package: hrgzdevengine-studio
Version: 1.0.0
Section: devel
Priority: optional
Architecture: all
Maintainer: HRGZDevEngine Team <support@hrgzdevengine.com>
Depends: nodejs (>= 16.0.0)
Description: All-in-One Game Creation, Compilation & Distribution System based on DOOM
 HRGZDevEngine Studio is a visual desktop game creator, compiler, and
 distribution hub for building standalone retro games powered by the DOOM engine.
`;
    fs.writeFileSync(path.join(controlDir, 'control'), control);

    // 2. Data payload: /usr/bin/hrgz-studio, /usr/bin/hrgz, /usr/share/...
    const usrBin = path.join(dataDir, 'usr', 'bin');
    const usrShare = path.join(dataDir, 'usr', 'share', 'hrgzdevengine-studio');
    const usrApps = path.join(dataDir, 'usr', 'share', 'applications');
    ensureDir(usrBin);
    ensureDir(usrShare);
    ensureDir(usrApps);

    // Copy studio files to /usr/share/hrgzdevengine-studio/
    fs.cpSync(path.join(REPO_ROOT, 'studio'), path.join(usrShare, 'studio'), { recursive: true });
    fs.cpSync(path.join(REPO_ROOT, 'bin'), path.join(usrShare, 'bin'), { recursive: true });

    // Launcher scripts
    const launcherSh = `#!/bin/sh
exec node /usr/share/hrgzdevengine-studio/studio/server.js "$@"
`;
    fs.writeFileSync(path.join(usrBin, 'hrgz-studio'), launcherSh);
    fs.chmodSync(path.join(usrBin, 'hrgz-studio'), 0o755);

    const cliSh = `#!/bin/sh
exec node /usr/share/hrgzdevengine-studio/bin/hrgz "$@"
`;
    fs.writeFileSync(path.join(usrBin, 'hrgz'), cliSh);
    fs.chmodSync(path.join(usrBin, 'hrgz'), 0o755);

    // Desktop entry
    const desktop = `[Desktop Entry]
Name=HRGZDevEngine Studio
Comment=DOOM Engine Game Creation & Distribution Studio
Exec=/usr/bin/hrgz-studio
Terminal=false
Type=Application
Categories=Development;Game;
`;
    fs.writeFileSync(path.join(usrApps, 'hrgzdevengine-studio.desktop'), desktop);

    // 3. Package control.tar.gz and data.tar.gz
    const debianBinary = path.join(debStaging, 'debian-binary');
    fs.writeFileSync(debianBinary, '2.0\n');

    const controlTar = path.join(debStaging, 'control.tar.gz');
    const dataTar = path.join(debStaging, 'data.tar.gz');

    run(`tar -czf "${controlTar}" -C "${controlDir}" .`);
    run(`tar -czf "${dataTar}" -C "${dataDir}" .`);

    // 4. Combine into .deb using ar (-q -S to prevent symbol table generation)
    const debOutput = path.join(DIST_DIR, 'hrgzdevengine-studio_1.0.0_all.deb');
    if (fs.existsSync(debOutput)) fs.unlinkSync(debOutput);

    run(`ar -q -S "${debOutput}" debian-binary control.tar.gz data.tar.gz`, debStaging);

    console.log(`[SUCCESS] Linux Debian package (.deb) created: ${debOutput} (${(fs.statSync(debOutput).size / 1024).toFixed(1)} KB)\n`);
    return debOutput;
}

function packageWinExe() {
    console.log('\n=======================================================');
    console.log(' [Windows] Building HRGZDevEngine Studio Dedicated .exe Bundle');
    console.log('=======================================================');

    ensureDir(DIST_DIR);
    ensureDir(BUILD_DIR);

    const winDir = path.join(DIST_DIR, 'HRGZDevEngine-Studio-Windows');
    if (fs.existsSync(winDir)) fs.rmSync(winDir, { recursive: true, force: true });
    ensureDir(winDir);

    // Copy studio files & root launchers
    fs.cpSync(path.join(REPO_ROOT, 'studio'), path.join(winDir, 'studio'), { recursive: true });
    fs.cpSync(path.join(REPO_ROOT, 'bin'), path.join(winDir, 'bin'), { recursive: true });
    fs.copyFileSync(path.join(REPO_ROOT, 'hrgz-studio.bat'), path.join(winDir, 'HRGZDevEngine-Studio.bat'));

    // Check if MinGW cross-compiler is available to compile native studio_win.c
    let mingw = false;
    try {
        execSync('which x86_64-w64-mingw32-gcc 2>/dev/null', { stdio: 'ignore' });
        mingw = true;
    } catch {}

    const srcFile = path.join(REPO_ROOT, 'studio', 'native', 'win', 'studio_win.c');
    const exeOut = path.join(winDir, 'HRGZDevEngine-Studio.exe');

    if (mingw) {
        console.log('[WIN] Compiling native Win32 GUI executable (HRGZDevEngine-Studio.exe)...');
        run(`x86_64-w64-mingw32-gcc -O3 -mwindows "${srcFile}" -o "${exeOut}" -lkernel32 -luser32 -lshell32`);
    } else {
        console.log('[WIN] MinGW cross-compiler not on host; compiling native C stub or copying Windows launcher.');
        // Provide dedicated VBS/PowerShell launcher that runs without console window
        const vbs = `Set WshShell = CreateObject("WScript.Shell")
WshShell.Run "cmd /c ""%~dp0HRGZDevEngine-Studio.bat""", 0, False
`;
        fs.writeFileSync(path.join(winDir, 'HRGZDevEngine-Studio.vbs'), vbs);
    }

    // Zip Windows release
    const zipOutput = path.join(DIST_DIR, 'HRGZDevEngine-Studio-Windows.zip');
    if (fs.existsSync(zipOutput)) fs.unlinkSync(zipOutput);
    run(`zip -r -y "${zipOutput}" HRGZDevEngine-Studio-Windows`, DIST_DIR);

    console.log(`[SUCCESS] Windows distribution created: ${winDir}\n`);
    return zipOutput;
}

function main() {
    const target = process.argv[2] || 'all';

    if (target === 'mac' || target === 'all') {
        packageMacDMG();
    }
    if (target === 'linux' || target === 'all') {
        packageLinuxDeb();
    }
    if (target === 'win' || target === 'all') {
        packageWinExe();
    }

    console.log('=======================================================');
    console.log(' All requested distribution packages generated successfully!');
    console.log(` Output directory: ${DIST_DIR}`);
    console.log('=======================================================\n');
}

main();
