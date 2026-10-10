/**
 * HRGZDevEngine Studio - Client Application Controller
 */

let currentProject = {};

// DOM Elements
const navButtons = document.querySelectorAll('.nav-btn');
const tabPanes = document.querySelectorAll('.tab-pane');

const inputTitle = document.getElementById('inputTitle');
const inputId = document.getElementById('inputId');
const inputVersion = document.getElementById('inputVersion');
const inputAuthor = document.getElementById('inputAuthor');
const inputDescription = document.getElementById('inputDescription');
const inputWebsite = document.getElementById('inputWebsite');
const inputWadPath = document.getElementById('inputWadPath');
const selectResolution = document.getElementById('selectResolution');
const selectMode = document.getElementById('selectMode');

const displayGameTitle = document.getElementById('displayGameTitle');
const displayVersion = document.getElementById('displayVersion');
const displayDescription = document.getElementById('displayDescription');
const displayId = document.getElementById('displayId');
const displayAuthor = document.getElementById('displayAuthor');
const displayWad = document.getElementById('displayWad');
const displayMode = document.getElementById('displayMode');

const systemText = document.getElementById('systemText');
const termLogs = document.getElementById('termLogs');
const btnRunGame = document.getElementById('btnRunGame');

// Tab Switching
navButtons.forEach(btn => {
    btn.addEventListener('click', () => {
        navButtons.forEach(b => b.classList.remove('active'));
        tabPanes.forEach(p => p.classList.remove('active'));

        btn.classList.add('active');
        const tabId = btn.getAttribute('data-tab');
        const pane = document.getElementById(tabId);
        if (pane) pane.classList.add('active');
    });
});

// Load System & Project
async function init() {
    try {
        const sysRes = await fetch('/api/system');
        const sys = await sysRes.json();
        const compilers = [];
        if (sys.tools.clang) compilers.push('Clang');
        if (sys.tools.gcc) compilers.push('GCC');
        if (sys.tools.mingw) compilers.push('MinGW');
        systemText.textContent = `${sys.platform.toUpperCase()} (${compilers.join(', ') || 'Native'})`;

        const projRes = await fetch('/api/project');
        currentProject = await projRes.json();
        populateForm(currentProject);
        updateSummary(currentProject);

        // Auto inspect default WAD
        inspectWad(currentProject.wadPath || 'doom1.wad');
        checkGameStatus();
    } catch (err) {
        console.error('Initialization error:', err);
    }
}

function populateForm(proj) {
    inputTitle.value = proj.title || '';
    inputId.value = proj.id || '';
    inputVersion.value = proj.version || '1.0.0';
    inputAuthor.value = proj.author || '';
    inputDescription.value = proj.description || '';
    inputWebsite.value = proj.website || '';
    inputWadPath.value = proj.wadPath || 'doom1.wad';
    if (proj.defaultResolution) selectResolution.value = proj.defaultResolution;
    if (proj.gameMode) selectMode.value = proj.gameMode;
}

function updateSummary(proj) {
    displayGameTitle.textContent = proj.title || 'Untitled Game';
    displayVersion.textContent = `v${proj.version || '1.0.0'}`;
    displayDescription.textContent = proj.description || 'No description provided.';
    displayId.textContent = proj.id || '--';
    displayAuthor.textContent = proj.author || 'Anonymous';
    displayWad.textContent = proj.wadPath || 'doom1.wad';
    displayMode.textContent = (proj.gameMode || 'Shareware').toUpperCase();
}

function readForm() {
    return {
        ...currentProject,
        title: inputTitle.value.trim() || 'Untitled Game',
        id: inputId.value.trim().toLowerCase().replace(/[^a-z0-9_-]/g, '') || 'mygame',
        version: inputVersion.value.trim() || '1.0.0',
        author: inputAuthor.value.trim() || 'Indie Studio',
        description: inputDescription.value.trim(),
        website: inputWebsite.value.trim(),
        wadPath: inputWadPath.value.trim() || 'doom1.wad',
        defaultResolution: selectResolution.value,
        gameMode: selectMode.value
    };
}

// Save Project
document.getElementById('btnSaveProject').addEventListener('click', async () => {
    const updated = readForm();
    try {
        const res = await fetch('/api/project', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(updated)
        });
        const data = await res.json();
        if (data.ok) {
            currentProject = data.project;
            updateSummary(currentProject);
            alert('Project configuration saved successfully!');
        }
    } catch (err) {
        alert('Failed to save project: ' + err.message);
    }
});

// Inspect WAD
async function inspectWad(wadPath) {
    try {
        const res = await fetch('/api/wad/inspect', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ wadPath })
        });
        const data = await res.json();
        if (data.ok) {
            renderWadInspection(data.wad);
        } else {
            document.getElementById('wadStatusBanner').textContent = 'Error: ' + data.error;
        }
    } catch (err) {
        document.getElementById('wadStatusBanner').textContent = 'Inspection failed: ' + err.message;
    }
}

function renderWadInspection(wad) {
    const banner = document.getElementById('wadStatusBanner');
    banner.textContent = `${wad.fileName} (${wad.fileSizeFormatted}) - ${wad.validation.statusMessage}`;

    document.getElementById('metricLumps').textContent = wad.numLumps;
    document.getElementById('metricMaps').textContent = wad.stats.mapCount;
    document.getElementById('metricSprites').textContent = wad.stats.spriteCount;
    document.getElementById('metricAudio').textContent = wad.stats.soundCount + wad.stats.musicCount;

    const mapTags = document.getElementById('mapTags');
    mapTags.innerHTML = '';
    wad.stats.maps.forEach(m => {
        const span = document.createElement('span');
        span.className = 'map-tag';
        span.textContent = m;
        mapTags.appendChild(span);
    });
}

document.getElementById('btnInspectWad').addEventListener('click', () => {
    inspectWad(inputWadPath.value.trim());
});

// Build Native Game
async function buildTarget(target) {
    // Switch to build tab
    const buildBtn = document.querySelector('[data-tab="tab-build"]');
    if (buildBtn) buildBtn.click();

    termLogs.textContent = `[STUDIO] Starting build task for target: ${target}...\n`;

    const project = readForm();

    try {
        const res = await fetch('/api/build', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ project, target })
        });

        const reader = res.body.getReader();
        const decoder = new TextDecoder();

        while (true) {
            const { done, value } = await reader.read();
            if (done) break;
            termLogs.textContent += decoder.decode(value);
            termLogs.scrollTop = termLogs.scrollHeight;
        }
    } catch (err) {
        termLogs.textContent += `\n[BUILD ERROR] ${err.message}\n`;
    }
}

document.getElementById('btnBuildProject').addEventListener('click', () => {
    buildTarget('mac');
});

// Distribute / Package
async function distTarget(target) {
    const buildBtn = document.querySelector('[data-tab="tab-build"]');
    if (buildBtn) buildBtn.click();

    termLogs.textContent += `\n[STUDIO] Starting packaging task for target: ${target}...\n`;
    const project = readForm();

    try {
        const res = await fetch('/api/dist', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ project, target })
        });

        const reader = res.body.getReader();
        const decoder = new TextDecoder();

        while (true) {
            const { done, value } = await reader.read();
            if (done) break;
            termLogs.textContent += decoder.decode(value);
            termLogs.scrollTop = termLogs.scrollHeight;
        }
    } catch (err) {
        termLogs.textContent += `\n[DIST ERROR] ${err.message}\n`;
    }
}

function clearConsole() {
    termLogs.textContent = 'Console cleared.\n';
}

// Run / Test Game
btnRunGame.addEventListener('click', async () => {
    const project = readForm();
    try {
        const res = await fetch('/api/run', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ project })
        });
        const data = await res.json();
        if (data.ok) {
            btnRunGame.classList.add('running');
            btnRunGame.textContent = 'Stop Game';
        } else {
            alert('Cannot run game: ' + data.error);
        }
    } catch (err) {
        alert('Run error: ' + err.message);
    }
});

async function checkGameStatus() {
    try {
        const res = await fetch('/api/status');
        const data = await res.json();
        if (data.running) {
            btnRunGame.textContent = 'Game Running (Click to Stop)';
            btnRunGame.classList.add('running');
        } else {
            btnRunGame.textContent = 'Play / Test Game';
            btnRunGame.classList.remove('running');
        }
    } catch {}
    setTimeout(checkGameStatus, 2000);
}

// Start
init();

