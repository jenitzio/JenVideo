// Cutline desktop shell (Electron 22 = last version supporting Windows 7).
const { app, BrowserWindow, Menu } = require('electron');
const path = require('path');

if (process.platform === 'linux') app.commandLine.appendSwitch('no-sandbox'); // AppImage cannot use SUID sandbox
app.commandLine.appendSwitch('autoplay-policy', 'no-user-gesture-required');

function create() {
  const win = new BrowserWindow({
    width: 1400, height: 880, minWidth: 960, minHeight: 600,
    backgroundColor: '#0e0f12', title: 'Cutline',
    webPreferences: { contextIsolation: true, nodeIntegration: false }
  });
  Menu.setApplicationMenu(null);
  win.loadFile(path.join(__dirname, 'index.html'));
}

app.whenReady().then(create);
app.on('window-all-closed', () => app.quit());
