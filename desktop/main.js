const electron = require('electron');
const app = electron.app;
const BrowserWindow = electron.BrowserWindow;

const path = require('path');
const url = require('url');
var process = require('process');
const settings = require('electron-settings');
var commandExists = require('command-exists').sync;
var fs = require('fs');

// Keep a global reference of the window object, if you don't, the window will
// be closed automatically when the JavaScript object is garbage collected.
var mainWindow;

function getDefaultEditorForPlatform() {
    if (process.platform === "darwin") {
        if (fs.existsSync('/Applications/CLion.app')) {
            return { name: "CLion", cmd: "/Applications/CLion.app/Contents/MacOS/clion" }
        } else if (commandExists('xed')) {
            return { name: "xed", cmd: "xed"}
        }
    } else if (process.platform === "win32") {
        // TODO define some default crap for windows
    } else if (process.platform === "linux") {
        // TODO define some default crap for linux
    }
    return { name: "none", cmd: ""};
}

function createWindow () {
    // Create the browser window.
    var mainScreen = electron.screen.getPrimaryDisplay();
    mainWindow = new BrowserWindow({width: mainScreen.size.width, height: mainScreen.size.height,
        titleBarStyle: 'hiddenInset',
        icon: path.join(__dirname, 'assets/icons/icon64.png')
    });

    // and load the index.html of the app.
    mainWindow.loadURL(url.format({
        pathname: path.join(__dirname, 'index.html'),
        protocol: 'file:',
        slashes: true
    }))

    // Open the DevTools.
    // mainWindow.webContents.openDevTools()

    // Emitted when the window is closed.
    mainWindow.on('closed', function () {
        // Dereference the window object, usually you would store windows
        // in an array if your app supports multi windows, this is the time
        // when you should delete the corresponding element.
        mainWindow = null
    })

    console.log(process.pid);
    require('./js/menu/mainmenu');
    //mainWindow.openDevTools()

    if (!settings.has('editor')) {
        settings.set('editor', getDefaultEditorForPlatform())
    }
}

// This method will be called when Electron has finished
// initialization and is ready to create browser windows.
// Some APIs can only be used after this event occurs.
app.on('ready', createWindow);

// Quit when all windows are closed.
app.on('window-all-closed', function () {
    // On OS X it is common for applications and their menu bar
    // to stay active until the user quits explicitly with Cmd + Q
    if (process.platform !== 'darwin') {
        app.quit()
    }
});

app.on('activate', function () {
    // On OS X it's common to re-create a window in the app when the
    // dock icon is clicked and there are no other windows open.
    if (mainWindow === null) {
        createWindow()
    }
})

// In this file you can include the rest of your app's specific main process
// code. You can also put them in separate files and require them here.
