
//===-- index.js ------------------------------------------------===//
//
//                     Memoro
//
// This file is distributed under the MIT License. 
// See LICENSE for details.
//
//===----------------------------------------------------------------------===//
//
// This file is a part of Memoro.
// Stuart Byma, EPFL.
//
//===----------------------------------------------------------------------===//

var remote = require("electron").remote;
const {ipcRenderer} = require('electron')
const settings = require('electron').remote.require('electron-settings');
//var Menu = remote.require('menu');

var chunk_graph = require("./js/chunkgraph");

function initApp() {
    addAppEventListeners();
    //chunk_graph.updateData("hplgst.json");
/*    document.getElementById("filter-form")
        .addEventListener("keyup", function(event) {
            event.preventDefault();
            if (event.keyCode === 13) {
                document.querySelector("#filter-button").click();
            }
        });*/

    // add div
    var element = document.querySelector("#overlay");
    element.style.visibility = "visible";

    if (!settings.has('theme')) {
        // a default
        console.log("did not have a theme in settings");
        settings.set('theme', 'light')
    }
    if (settings.get('theme') === 'light') {
        $('head link#bootstrapSheet').attr('href', 'css/light.css');
        $('head link#colorSheet').attr('href', 'css/chunk_graph_light.css');
    } else {
        $('head link#bootstrapSheet').attr('href', 'css/slate.css');
        $('head link#colorSheet').attr('href', 'css/chunk_graph_dark.css');
    }
}

function resetTimeClick() {
    chunk_graph.resetTimeClick();
}

function chunkScroll() {
    chunk_graph.chunkScroll();
}

function stackFilterClick() {
    chunk_graph.stackFilterClick();
}

function stackFilterResetClick() {
    chunk_graph.stackFilterResetClick();
}

function typeFilterClick() {
    chunk_graph.typeFilterClick();
}

function filterExecuteClick() {
    chunk_graph.filterExecuteClick();
}
function typeFilterResetClick() {
    chunk_graph.typeFilterResetClick();
}

function filterHelpClick() {
    chunk_graph.showFilterHelp();
}

function fgAllocationsClick() {
    console.log("allocations click")
    chunk_graph.setFlameGraphNumAllocs();
}

function fgBytesTimeClick() {
    console.log("bytes time click")
    chunk_graph.setFlameGraphBytesTime();
}

function fgBytesTotalClick() {
    console.log("bytes total click")
    chunk_graph.setFlameGraphBytesTotal();
}

function fgMostActiveClick() {
    console.log("most active click")
}

function fgHelpClick() {
    console.log("fg help click")
    chunk_graph.flameGraphHelp();
}

function globalInfoHelpClick() {
    console.log("fg help click")
    chunk_graph.globalInfoHelp();
}

function tabSwitchClick() {

    console.log("tab switch click");
    chunk_graph.tabSwitchClick();
}

function traceSort(pred) {
    chunk_graph.traceSort(pred);
}

function addAppEventListeners() {

    ipcRenderer.on('open_file', function(emitter, file_path) {
        console.log(file_path[0]);
        chunk_graph.updateData(file_path[0]);
    });

    ipcRenderer.on('theme_light', function(emitter) {
        console.log("changing theme to light!!");
        $('head link#bootstrapSheet').attr('href', 'css/light.css');
        $('head link#colorSheet').attr('href', 'css/chunk_graph_light.css');
        settings.set('theme', 'light')
    });

    ipcRenderer.on('theme_dark', function(emitter) {
        console.log("changing theme to dark!!");
        $('head link#bootstrapSheet').attr('href', 'css/slate.css');
        $('head link#colorSheet').attr('href', 'css/chunk_graph_dark.css');
        settings.set('theme', 'dark')
    });
}
