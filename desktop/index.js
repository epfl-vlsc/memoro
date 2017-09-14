var remote = require("electron").remote;
const {ipcRenderer} = require('electron')
//var Menu = remote.require('menu');

var chunk_graph = require("./js/chunkgraph");

function initApp() {
    addAppEventListeners();
    //chunk_graph.updateData("hplgst.json");
    document.getElementById("filter-form")
        .addEventListener("keyup", function(event) {
            event.preventDefault();
            if (event.keyCode === 13) {
                document.querySelector("#filter-button").click();
            }
        });

    // add div
    var element = document.querySelector("#overlay");
    element.style.visibility = "visible";
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
        chunk_graph.updateData(file_path[0]);
    });
}
