var remote = require("electron").remote;
const {ipcRenderer} = require('electron')
//var Menu = remote.require('menu');

var chunk_graph = require("./js/chunkgraph");

function initApp() {
    addAppEventListeners();
    chunk_graph.updateData("hplgst.json");
}

function addAppEventListeners() {

    ipcRenderer.on('open_file', function(emitter, file_path) {
        console.log(file_path[0]);
        chunk_graph.updateData(file_path[0]);
    });
}
