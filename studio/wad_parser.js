/**
 * HRGZDevEngine DOOM WAD Parser & Asset Validator
 * Reads IWAD/PWAD lump directories, inspects map structures, graphics,
 * sound effects, and validates standalone game asset requirements.
 */

const fs = require('fs');

function parseWad(filePath) {
    if (!fs.existsSync(filePath)) {
        throw new Error(`File not found: ${filePath}`);
    }

    const stat = fs.statSync(filePath);
    const fd = fs.openSync(filePath, 'r');
    const headerBuf = Buffer.alloc(12);
    fs.readSync(fd, headerBuf, 0, 12, 0);

    const type = headerBuf.toString('ascii', 0, 4);
    if (type !== 'IWAD' && type !== 'PWAD') {
        fs.closeSync(fd);
        throw new Error(`Invalid DOOM WAD format: identification is '${type}' (expected IWAD or PWAD)`);
    }

    const numLumps = headerBuf.readInt32LE(4);
    const dirOffset = headerBuf.readInt32LE(8);

    if (numLumps < 0 || numLumps > 50000 || dirOffset < 12 || dirOffset > stat.size) {
        fs.closeSync(fd);
        throw new Error(`Corrupt WAD directory: ${numLumps} lumps at offset ${dirOffset}`);
    }

    // Read directory
    const dirSize = numLumps * 16;
    const dirBuf = Buffer.alloc(dirSize);
    fs.readSync(fd, dirBuf, 0, dirSize, dirOffset);
    fs.closeSync(fd);

    const lumps = [];
    const maps = new Set();
    let hasPlaypal = false;
    let hasColormap = false;
    let hasTexture1 = false;
    let hasPnames = false;
    let spriteCount = 0;
    let flatCount = 0;
    let soundCount = 0;
    let musicCount = 0;

    let inSprites = false;
    let inFlats = false;

    for (let i = 0; i < numLumps; i++) {
        const offset = dirBuf.readInt32LE(i * 16);
        const size = dirBuf.readInt32LE(i * 16 + 4);
        let name = dirBuf.toString('ascii', i * 16 + 8, i * 16 + 16).replace(/\0+$/, '').toUpperCase();

        lumps.push({ index: i, name, offset, size });

        // Markers
        if (name === 'S_START' || name === 'SS_START') inSprites = true;
        else if (name === 'S_END' || name === 'SS_END') inSprites = false;
        else if (name === 'F_START' || name === 'FF_START') inFlats = true;
        else if (name === 'F_END' || name === 'FF_END') inFlats = false;

        // Lump classification
        if (name === 'PLAYPAL') hasPlaypal = true;
        if (name === 'COLORMAP') hasColormap = true;
        if (name === 'TEXTURE1') hasTexture1 = true;
        if (name === 'PNAMES') hasPnames = true;

        if (name.startsWith('DS')) soundCount++;
        if (name.startsWith('D_')) musicCount++;
        if (inSprites && size > 0) spriteCount++;
        if (inFlats && size > 0) flatCount++;

        // Map detection: E#M# or MAP##
        if (/^E[1-4]M[1-9]$/.test(name) || /^MAP(0[1-9]|[1-2][0-9]|3[0-2])$/.test(name)) {
            maps.add(name);
        }
    }

    const isStandalone = type === 'IWAD' && hasPlaypal && hasColormap && (hasTexture1 || maps.size > 0);

    return {
        path: filePath,
        fileName: filePath.split(/[/\\]/).pop(),
        fileSizeBytes: stat.size,
        fileSizeFormatted: (stat.size / (1024 * 1024)).toFixed(2) + ' MB',
        type,
        numLumps,
        isStandalone,
        validation: {
            hasPlaypal,
            hasColormap,
            hasTexture1,
            hasPnames,
            isPlayableBase: isStandalone,
            statusMessage: isStandalone
                ? 'Valid Standalone Base IWAD (Contains palette, colormaps, and required game assets)'
                : type === 'PWAD'
                    ? 'Valid PWAD Addon / Mod Pack (Requires a Base IWAD to run)'
                    : 'Partial IWAD (May require supplementary assets for textures or palette)'
        },
        stats: {
            mapCount: maps.size,
            maps: Array.from(maps).sort(),
            spriteCount,
            flatCount,
            soundCount,
            musicCount
        },
        sampleLumps: lumps.slice(0, 100)
    };
}

module.exports = { parseWad };

