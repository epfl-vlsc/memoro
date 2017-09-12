var remote = require("electron").remote;
const {ipcRenderer} = require('electron')
//var Menu = remote.require('menu');

var chunk_graph = require("./js/chunkgraph");

function initApp() {
    var element = document.querySelector("#loadspinner");
    element.style.visibility = "hidden";
    addAppEventListeners();
    //chunk_graph.updateData("hplgst.json");
}

function stackFilterClick() {
    chunk_graph.stackFilterClick();
}

function resetTimeClick() {
    chunk_graph.resetTimeClick();
}

function chunkScroll() {
    chunk_graph.chunkScroll();
}

function filterHelpClick() {
    chunk_graph.showFilterHelp();
}

function stackFilterResetClick() {
    chunk_graph.stackFilterResetClick();
}

function addAppEventListeners() {

    ipcRenderer.on('open_file', function(emitter, file_path) {
        console.log(file_path[0]);
        var element = document.querySelector("#loadspinner");
        element.style.visibility = "visible";
        chunk_graph.updateData(file_path[0]);
    });
}
