//===-- chunkgraph.js ------------------------------------------------===//
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





// bear in mind that I don't _really_ do javascript





global.d3 = require("d3");
const exec = require('child_process').exec;
require("../node_modules/d3-flame-graph/dist/d3.flameGraph");
require("../node_modules/d3-tip/index");
var memoro = require('../cpp/build/Release/memoro.node');
var path = require('path');
var fs = require("fs");
var gmean = require('compute-gmean');

const settings = require('electron').remote.require('electron-settings');

function formatBignum(num) {
    return num.toString().replace(/(\d)(?=(\d{3})+(?!\d))/g, '$1,');
}

function bytesToString(bytes,decimals) {
    if(bytes == 0) return '0 B';
    var k = 1024,
        dm = decimals || 2,
        sizes = ['B', 'KB', 'MB', 'GB', 'TB', 'PB', 'EB', 'ZB', 'YB'],
        i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + ' ' + sizes[i];
}

var bytesToStringNoDecimal = function (bytes) {

    var fmt = d3.format('.0f');
    if (bytes < 1024) {
        return fmt(bytes) + 'B';
    } else if (bytes < 1024 * 1024) {
        return fmt(bytes / 1024) + 'kB';
    } else if (bytes < 1024 * 1024 * 1024) {
        return fmt(bytes / 1024 / 1024) + 'MB';
    } else {
        return fmt(bytes / 1024 / 1024 / 1024) + 'GB';
    }
}

// convenience globals
var barHeight;
var total_chunks;

var chunk_graph_width;
var chunk_graph_height;
var aggregate_graph_height;
var chunk_y_axis_space;
var x;  // the x range
var global_x;  // the global tab x range
var y;  // aggregate y range
var max_x;
var num_traces;
var current_fg_type = "num_allocs";
var current_fg_time = 0;

var avg_lifetime;
var avg_usage;
var avg_useful_life;
var lifetime_var;
var usage_var;
var useful_life_var;

var aggregate_max = 0;
var filter_words = [];

var tree;
var hideNode;
var showHideNode = false;

var colors = [
"#b0c4de",
"#b0c4de",
"#a1b8d4",
"#93acca",
"#84a0c0",
"#7594b6",
"#6788ac",
"#587ba2",
"#496f98",
"#3b638e",
"#2c5784",
"#1d4b7a",
"#0f3f70",
"#003366"];

var badness_colors = [
    "#0085ff",
    "#00cfff",
    "#00ffe5",
    "#00ff9b",
    "#00ff51",
    "#00ff07",
    "#43ff00",
    "#8cff00",
    "#d6ff00",
    "#ffde00",
    "#ff9400",
    "#ff4a00",
    "#ff0000"];

// chunk color gradient scale
var colorScale = d3.scaleQuantile()
.range(colors);

function showLoader() {
    var div = document.createElement("div");
    div.id = "loadspinner";
    div.className = "spinny-load";
    div.html = "Loading ...";
    document.getElementById("middle-column").appendChild(div);
}

function hideLoader() {
    // completely remove, otherwise it takes CPU cycles while
    // animating, even if hidden?
    var element = document.getElementById("loadspinner");
    element.parentNode.removeChild(element);
}

function showModal(title, body, icon = "") {
    $("#modal-title-text").text(" " + title);
    var ic = $("#modal-icon");
    ic.removeClass();
    if (icon !== '')
        ic.addClass("fa " + icon);
    $(".modal-body").html(body);
    $("#main-modal").modal("show")
}
// file open callback function
function updateData(datafile) {

    showLoader();

    var folder = path.dirname(datafile);
    var filename = datafile.replace(/^.*[\\\/]/, '');
    var idx = filename.lastIndexOf('.');
    var name = filename.substring(0, idx);
    if (filename.endsWith(".stats")) {
        var stats_path = folder + "/" + name + ".stats";
        console.log("stats " + stats_path);
        memoro.set_dataset_stats(folder+'/', stats_path, set_dataset_done);
    } else {
        var trace_path = folder + "/" + name + ".trace";
        var chunk_path = folder + "/" + name + ".chunks";
        console.log("trace " + trace_path + " chunk path " + chunk_path);
        // set dataset is async, because it can take some time with large trace files
        memoro.set_dataset(folder+'/', trace_path, chunk_path, set_dataset_done);
    }
}

function set_dataset_done(result) {
    hideLoader();
    //console.log(result);
    if (!result.result) {
        showModal("Error", "File parsing failed with error: " + result.message, "fa-exclamation-triangle");
    } else {

        // add default "main" filter?
        drawEverything();
    }
    var element = document.querySelector("#overlay");
    element.style.visibility = "hidden";
    updateActiveFGType();
}

function badnessTooltip(idx) {

    return function() {
        var div = d3.select("#tooltip");
        div.transition()
            .duration(200)
            .style("opacity", .9);
        div.html("")
            .style("left", (d3.event.pageX) + "px")
            .style("top", (d3.event.pageY - 28) + "px");
        var svg = div.append("svg")
            .attr("width", 10*badness_colors.length)
            .attr("height", 15);

        badness_colors.forEach(function(c, i) {
            var circ = svg.append("circle")
                .attr("transform", "translate(" + (5+i*10) + ", 8)")
                .attr("r", 5)
                .style("fill", c);
            if (idx === i)
                circ.classed("indicator-circle", true)
        })

        //div.attr("width", width);
    }
}
function constructInferences(inef) {
    var ret = "";
    if (inef.unused)
        ret += "<i class=\"fa fa-chevron-right\" aria-hidden=\"true\"></i> unused chunks </br>"
    if (!inef.unused) {
        if (inef.read_only)
            ret += "<i class=\"fa fa-chevron-right\" aria-hidden=\"true\"></i> read-only chunks </br>"
        if (inef.write_only)
            ret += "<i class=\"fa fa-chevron-right\" aria-hidden=\"true\"></i> write-only chunks </br>"
        if (inef.short_lifetime)
            ret += "<i class=\"fa fa-chevron-right\" aria-hidden=\"true\"></i> short lifetime chunks </br>"
        if (!inef.short_lifetime) {
            if (inef.late_free)
                ret += "<i class=\"fa fa-chevron-right\" aria-hidden=\"true\"></i> chunks freed late </br>"
            if (inef.early_alloc)
                ret += "<i class=\"fa fa-chevron-right\" aria-hidden=\"true\"></i> chunks allocated early </br>"
        }
        if (inef.increasing_allocs)
            ret += "<i class=\"fa fa-chevron-right\" aria-hidden=\"true\"></i> increasing allocations </br>"
        if (inef.top_percentile_chunks)
            ret += "<i class=\"fa fa-chevron-right\" aria-hidden=\"true\"></i> in top-% chunks alloc'd </br>"
        if (inef.top_percentile_size)
            ret += "<i class=\"fa fa-chevron-right\" aria-hidden=\"true\"></i> in top-% bytes alloc'd </br>"
        if (inef.multi_thread)
            ret += "<i class=\"fa fa-chevron-right\" aria-hidden=\"true\"></i> multithread accesses </br>"
        if (inef.low_access_coverage)
            ret += "<i class=\"fa fa-chevron-right\" aria-hidden=\"true\"></i> low access coverage </br>"
    }

    return ret;
}

var current_sort_order = 'bytes';
function sortTraces() {
    memoro.sort_traces(current_sort_order);
}

function generateOpenSourceCmd(file, line) {
    var editor = settings.get('editor');
    if (editor.name === "none") {
        showModal("Error", "No editor defined, and no defaults found on system.", "fa-exclamation-triangle");
        return "";
    }
    if (editor.name === "CLion") {
        return editor.cmd + " " + file + " --line " + line + " " + file;
    } else if (editor.name === "xed") {
        return editor.cmd + " --line " + line + " " + file;
    } else {
        return "";
    }
    // TODO add other supported editors here
}

function generateTraceHtml(raw) {
    // parse each trace for source file and line numbers,
    // add ondblclick events to open in desired editor
    var traces = raw.split('|');

    var trace = document.getElementById("trace");
    // remove existing
    while (trace.firstChild) {
        trace.removeChild(trace.firstChild);
    }

    traces.forEach(function (t) {
        if (t === "") return;
        var p = document.createElement("p");
        p.innerHTML = t + "  ";
        // yet more stuff depending on exact trace formatting
        // eventually we need to move symbolizer execution to the visualizer
        // i.e. *here* and have the compiler RT store only binary addrs
        var cmd = "";
        var line = "";
        var file = "";
        var file_line = t.substr(t.indexOf('/'));
        var last_colon = file_line.lastIndexOf(':');
        var second_last_colon = file_line.lastIndexOf(':', last_colon-1);
        // TODO clean up this parsing to something more readable
        if (second_last_colon !== -1) {
            line = file_line.substring(second_last_colon+1, last_colon);
            if (isNaN(line)){
                line = file_line.substr(last_colon+1);
                if (!isNaN(line)) {
                    file = file_line.substring(0, last_colon);
                    cmd = generateOpenSourceCmd(file, line);
                }
            } else {
                file = file_line.substring(0, second_last_colon);
                //cmd = '/Applications/CLion.app/Contents/MacOS/clion ' + file + ' --line ' + line + " " + file;
                cmd = generateOpenSourceCmd(file, line);
            }
        } else {
            line = file_line.substr(last_colon+1);
            if (!isNaN(line)) {
                file = file_line.substring(0, last_colon);
                //cmd = '/Applications/CLion.app/Contents/MacOS/clion ' + file + ' --line ' + line + " " + file;
                cmd = generateOpenSourceCmd(file, line);
            }
        }

/*        p.ondblclick = function () {
/!*            console.log("file is " + file + ", line is " + line);
            console.log("cmd is: " + cmd);*!/
            if (!fs.existsSync(file)) {
                showModal("Error", "Cannot locate file: " + file);
            }
            if (cmd !== "") {
                exec(cmd, function(err, stdout, stderr) {
                    if (err) {
                        // node couldn't execute the command
                        console.log("could not open text editor " + stdout + "\n" + stderr);
                        showModal("Error", "Open editor failed with stderr: " + stderr);
                    }
                });
            }
        };*/
        var btn = document.createElement("i");
        btn.className += "fa fa-folder-open";
        btn.style.visibility = "hidden";
        btn.onclick = function () {
            /*            console.log("file is " + file + ", line is " + line);
                        console.log("cmd is: " + cmd);*/
            if (!fs.existsSync(file)) {
                showModal("Error", "Cannot locate file: " + file, "fa-exclamation-triangle");
            }
            if (cmd !== "") {
                exec(cmd, function(err, stdout, stderr) {
                    if (err) {
                        // node couldn't execute the command
                        console.log("could not open text editor " + stdout + "\n" + stderr);
                        showModal("Error", "Open editor failed with stderr: " + stderr, "fa-exclamation-triangle");
                    }
                });
            }
        };
        btn.onmouseover = function() {
            btn.className = "fa fa-folder-open-o";
        };
        btn.onmouseout = function() {
            btn.className = "fa fa-folder-open";
        };
        p.appendChild(btn);
        p.onmouseover = function () {
            p.style.fontWeight = 'bold';
            p.getElementsByClassName("fa")[0].style.visibility = "visible";
        };
        p.onmouseout = function () {
            p.style.fontWeight = 'normal';
            p.getElementsByClassName("fa")[0].style.visibility = "hidden";
        };
        var br = document.createElement("hr");
        trace.appendChild(p);
        trace.appendChild(br);
    })
}

var load_threshold = 90;
let stacktraces_count = 20;
var current_stacktrace_index = stacktraces_count;
var current_stacktrace_index_low = 0;

function removeStackTracesTop(num) {
    var svgs = d3.select('#traces').selectAll('svg')._groups[0];
    for (i = 0; i < num; i++)
        svgs[i].remove();
}

function removeStackTracesBottom(num) {
    var svgs = d3.select('#traces').selectAll('svg')._groups[0];
    var end = svgs.length;

    for (i = end - num; i < end; i++)
        svgs[i].remove();
}

function traceScroll() {
    var element = document.querySelector("#traces");
    var percent = 100 * element.scrollTop / (element.scrollHeight - element.clientHeight);
    if (percent > load_threshold) {
        var traces = memoro.traces(current_stacktrace_index, stacktraces_count);
        if (traces.length > 0) {
            // there are still some to display
            var to_append = traces.length;
            var to_remove = to_append;
            // remove from top
            for (var i = 0; i < to_append; i++) {
                var sampled = memoro.aggregate_trace(traces[i].trace_index);
                renderStackTraceSvg(traces[i], i, sampled, true);
            }
            current_stacktrace_index += to_append;
            current_stacktrace_index_low += to_append;

            removeStackTracesTop(to_remove);
        }
    } else if (percent < (100 - load_threshold)) {
        if (current_stacktrace_index_low > 0) {
            var traces = memoro.traces(
                Math.max(current_stacktrace_index_low - stacktraces_count, 0), // Don't go <0
                Math.min(stacktraces_count, current_stacktrace_index_low)); // Don't ask <0
            if (traces.length === 0)
                console.log("oh fuck traces is 0");
            var to_prepend = traces.length;
            var to_remove = to_prepend;

            for (i = traces.length-1; i >= 0; i--) {
                var sampled = memoro.aggregate_trace(traces[i].trace_index);
                renderStackTraceSvg(traces[i], i, sampled, false);
            }
            current_stacktrace_index -= to_prepend;
            current_stacktrace_index_low -= to_prepend;

            removeStackTracesBottom(to_remove);
        }
    }
}

function renderStackTraceSvg(d, i, sampled, bottom) {
    var trace = d3.select("#trace");
    var traces_div = d3.select("#traces");
    var peak = d.max_aggregate
    var rectHeight = 55;

    var new_svg_g;
    if (bottom)
        new_svg_g = traces_div.append("svg");
    else
        new_svg_g = traces_div.insert("svg", ":first-child");

    new_svg_g
        .attr("width", chunk_graph_width)
        .attr("height", rectHeight)
        .classed("svg_spacing_trace", true)
        .on("click", function(s) {
            clearChunks();
            if (d3.select(this).classed("selected")) {
                d3.selectAll(".select-rect").style("display", "none");
                d3.select(this).classed("selected", false);
                trace.html("Select an allocation point under \"Heap Allocation\"");
                var info = d3.select("#inferences");
                info.html("");
                d3.select("#stack-agg-path").remove();
            } else {
                colorScale.domain([1, peak]);

                generateTraceHtml(d.trace);

                d3.selectAll(".select-rect").style("display", "none");
                d3.select(this).selectAll(".select-rect").style("display", "inline");
                d3.select(this).classed("selected", true);
                drawChunks(d);
                var info = d3.select("#inferences");
                var inef = memoro.inefficiencies(d.trace_index);
                var html = constructInferences(inef);
                html += "</br>Usage: " + d.usage_score.toFixed(2) + "</br>Lifetime: " + d.lifetime_score.toFixed(2)
                    + "</br>Useful Lifetime: " + d.useful_lifetime_score.toFixed(2);
                info.html(html);

                var fresh_sampled = memoro.aggregate_trace(d.trace_index);
                var agg_line = d3.line()
                    .x(function(v) {
                        return x(v["ts"]);
                    })
                    .y(function(v) { return y(v["value"]); })
                    .curve(d3.curveStepAfter);

                d3.select("#stack-agg-path").remove();
                var aggregate_graph_g = d3.select("#aggregate-group");
                aggregate_graph_g.append("path")
                    .datum(fresh_sampled)
                    .attr("id", "stack-agg-path")
                    .attr("fill", "none")
                    .classed("stack-agg-graph", true)
                    .attr("stroke-width", 1.0)
                    .attr("transform", "translate(0, 5)")
                    .attr("d", agg_line);
            }
        })
        .append("g");

    new_svg_g.append("rect")
        .attr("transform", "translate(0, 0)")
        .attr("width", chunk_graph_width - chunk_y_axis_space)
        .attr("height", rectHeight)
        .attr("class", function(x) {
            if (i % 2 == 0) {
                return "background1";
            } else {
                return "background2";
            }
        });

    var mean = gmean([d.lifetime_score < 0.01 ? 0.01 : d.lifetime_score, d.usage_score < 0.01 ? 0.01 : d.usage_score,
        d.useful_lifetime_score < 0.01 ? 0.01 : d.useful_lifetime_score]);
    var badness_col = Math.round((1.0 - mean) * (badness_colors.length - 1));
    new_svg_g.append('circle')
        .attr("transform", "translate(" + (chunk_graph_width - chunk_y_axis_space - 15) + ", 4)")
        .attr("cx", 5)
        .attr("cy", 5)
        .attr("r", 5)
        .style("fill", badness_colors[badness_col])
        .on("mouseover", badnessTooltip(badness_col))
        .on("mouseout", function(d) {
            var div = d3.select("#tooltip");
            div.transition()
                .duration(500)
                .style("opacity", 0);
        });

    new_svg_g.append("rect")
        .attr("transform", "translate(0, 0)")
        .attr("width", chunk_graph_width - chunk_y_axis_space)
        .attr("height", rectHeight)
        .style("fill", "none")
        .style("stroke-width", "2px")
        .style("stroke", "steelblue")
        .style("display", "none")
        .classed("select-rect", true);

    //sampled = memoro.aggregate_trace(d.trace_index);

    var t = new_svg_g.append("text");
    t.style("font-size", "small")
        .classed("stack-text", true)
        .attr("x", 5)
        .attr("y", "15")
        .text("Chunks: " + (d.num_chunks) + ", Peak Bytes: " + bytesToString(peak) + " Type: " + d.type);


    var stack_y = d3.scaleLinear()
        .range([rectHeight-25, 0]);

    stack_y.domain(d3.extent(sampled, function(v) { return v["value"]; }));

    var yAxisRight = d3.axisRight(stack_y)
        .tickFormat(bytesToStringNoDecimal)
        .ticks(2);

    var line = d3.line()
        .x(function(v) {
            return x(v["ts"]);
        })
        .y(function(v) { return stack_y(v["value"]); })
        .curve(d3.curveStepAfter);

    new_svg_g.append("path")
        .datum(sampled)
        .attr("fill", "none")
        .classed("stack-agg-overlay", true)
        .attr("stroke-width", 1.0)
        .attr("transform", "translate(0, 20)")
        .attr("d", line);
    new_svg_g.append("g")
        .attr("class", "y axis")
        .attr("transform", "translate(" + (chunk_graph_width - chunk_y_axis_space + 1) + ",20)")
        .style("fill", "white")
        .call(yAxisRight);
}

function drawStackTraces() {

    var trace = d3.select("#trace");
    var traces_div = d3.select("#traces");
    traces_div.selectAll("svg").remove();

    traces_div.property('scrollTop', 0);

    d3.select("#inferences").html("");

    //var max_x = xMax();
    var max_x = memoro.filter_max_time();
    var min_x = memoro.filter_min_time();

    x.domain([min_x, max_x]);

    sortTraces();
    current_stacktrace_index_low = 0;
    current_stacktrace_index = stacktraces_count * 3;
    var traces = memoro.traces(current_stacktrace_index_low, current_stacktrace_index);
    num_traces = traces.length;
    total_chunks = 0;

    var total_usage = 0;
    var total_lifetime = 0;
    var total_useful_lifetime = 0;

    var cur_background_class = 0;
    traces.forEach(function (d, i) {
        total_usage += d.usage_score;
        total_lifetime += d.lifetime_score;
        total_useful_lifetime += d.useful_lifetime_score;

        var sampled = memoro.aggregate_trace(d.trace_index);
        total_chunks += d.num_chunks;

        renderStackTraceSvg(d, i, sampled, true);
    });

    avg_lifetime = total_lifetime / traces.length;
    avg_usage = total_usage / traces.length;
    avg_useful_life = total_useful_lifetime / traces.length;

    var lifetime_var_total = 0;
    var usage_var_total = 0;
    var useful_lifetime_var_total = 0;
    traces.forEach(function(t) {
        lifetime_var_total += Math.pow(t.lifetime_score - avg_lifetime, 2);
        usage_var_total += Math.pow(t.usage_score - avg_usage, 2);
        useful_lifetime_var_total += Math.pow(t.useful_lifetime_score - avg_useful_life, 2);
    });

    lifetime_var = lifetime_var_total / traces.length;
    usage_var = usage_var_total / traces.length;
    useful_life_var = useful_lifetime_var_total / traces.length;
}

function tooltip(chunk) {
    return function() {
        var div = d3.select("#tooltip")
        div.transition()
            .duration(200)
            .style("opacity", .9);
        div .html(bytesToString(chunk["size"]) + "</br> Reads: " + chunk["num_reads"]
            + "</br>Writes: " + chunk["num_writes"] + "</br>Access Ratio: " +
            ((chunk.access_interval_high - chunk.access_interval_low) / chunk.size).toFixed(1)
            + "</br>Access Interval: [" + chunk.access_interval_low + "," + chunk.access_interval_high
            + "]</br>MultiThread: " + chunk.multi_thread + "</br>")
            .style("left", (d3.event.pageX) + "px")
            .style("top", (d3.event.pageY - 100) + "px");
        div.append("svg")
            .attr("width", 10)
            .attr("height", 10)
            .append("rect")
            .attr("width", 10)
            .attr("height", 10)
            .style("fill", colorScale(chunk.size))
        //div.attr("width", width);
    }
}

function renderChunkSvg(chunk, text, bottom) {
    var min_x = memoro.filter_min_time()


    var chunk_div = d3.select("#chunks");
    var div = d3.select("#tooltip");
    var new_svg_g;
    if (bottom) {
        new_svg_g = chunk_div.append("div").append("svg");
    }
    else {
        new_svg_g = chunk_div.insert("div", ":first-child").append("svg")
    }

    new_svg_g.attr("width", chunk_graph_width)
        .attr("height", barHeight)
        .classed("svg_spacing_data", true);
    new_svg_g.append("rect")
        .attr("transform", "translate("+ x(chunk["ts_start"])  +",0)")
        .attr("width", Math.max(x(min_x + chunk["ts_end"] - chunk["ts_start"]), 3))
        .attr("height", barHeight)
        .style("fill", colorScale(chunk.size))
        .on("mouseover", tooltip(chunk))
        .on("mouseout", function(d) {
            div.transition()
                .duration(500)
                .style("opacity", 0);
        });
    /*new_svg_g.append("text")
        .attr("x", (chunk_graph_width - 130))
        .attr("y", 6)
        .text(text.toString())
        .classed("chunk_text", true);*/

    new_svg_g.append("line")
        .attr("y1", 0)
        .attr("y2", barHeight)
        .attr("x1", x(chunk["ts_first"]))
        .attr("x2", x(chunk["ts_first"]))
        .style("stroke-width", 2)
        .attr("display", chunk["ts_first"] === 0 ? "none" : null)
        .classed("firstlastbars", true);

    var next = 0;
    if (x(chunk["ts_last"]) - x(chunk["ts_first"]) < 1)
    {
        next = x(chunk["ts_first"]) + 3;
    } else {
        next = x(chunk["ts_last"]);
    }
    new_svg_g.append("line")
        .attr("y1", 0)
        .attr("y2", barHeight)
        .attr("x1", next)
        .attr("x2", next)
        .attr("display", chunk["ts_first"] === 0 ? "none" : null)
        .style("stroke-width", 2)
        .classed("firstlastbars", true);
}

function removeChunksTop(num) {
    var svgs = d3.select("#chunks").selectAll("div")._groups[0];

    for (i = 0; i < num; i++)
    {
        svgs[i].remove();
    }
}

function removeChunksBottom(num) {

    var svgs = d3.select("#chunks").selectAll("div")._groups[0];
    var end = svgs.length;

    //console.log(svgs)
    for (i = end - num; i < end; i++)
    {
        svgs[i].remove();
    }
}

var current_trace_index = null;
var current_chunk_index = 0;
var current_chunk_index_low = 0;

function chunkScroll() {
    var element = document.querySelector("#chunks");
    var percent = 100 * element.scrollTop / (element.scrollHeight - element.clientHeight);
    if (percent > load_threshold) {
        var chunks = memoro.trace_chunks(current_trace_index, current_chunk_index, 25);
        if (chunks.length > 0) {
            // there are still some to display
            var to_append = chunks.length;
            var to_remove = to_append;
            // remove from top
            for (var i = 0; i < to_append; i++) {
                renderChunkSvg(chunks[i], current_chunk_index + i, true);
            }
            current_chunk_index += to_append;
            current_chunk_index_low += to_append;

            removeChunksTop(to_remove);
        }
    } else if (percent < (100 - load_threshold)) {
        if (current_chunk_index_low > 0) {
            var chunks = memoro.trace_chunks(current_trace_index, Math.max(current_chunk_index_low-25, 0), Math.min(25, current_chunk_index_low));
            if (chunks.length === 0)
                console.log("oh fuck chunks is 0");
            var to_prepend = chunks.length;
            var to_remove = to_prepend;

            for (i = chunks.length-1; i >= 0; i--) {
                renderChunkSvg(chunks[i], current_chunk_index_low - (to_prepend - i), false);
            }
            current_chunk_index -= to_prepend;
            current_chunk_index_low -= to_prepend;

            removeChunksBottom(to_remove);
        }

    }
}

function clearChunks() {
    var chunk_div = d3.select("#chunks");

    chunk_div.selectAll("div").remove();
    current_trace_index = null;

    chunk_div.property('scrollTop', 0);
}

// draw the first X chunks from this trace
function drawChunks(trace) {

    // test if this is already present in the div
    if (current_trace_index === trace.trace_index)
        return;

    current_trace_index = trace.trace_index;

    var chunk_div = d3.select("#chunks");

    chunk_div.selectAll("div").remove();

    var max_x = memoro.filter_max_time();
    var min_x = memoro.filter_min_time();
    //console.log("min x " + min_x + " max x " + max_x)

    x.domain([min_x, max_x]);

    current_chunk_index_low = trace.chunk_index;
    current_chunk_index = Math.min(trace.num_chunks, 200);
    chunks  = memoro.trace_chunks(trace.trace_index, current_chunk_index_low, current_chunk_index);

    for (var i = 0; i < chunks.length; i++) {
        renderChunkSvg(chunks[i], current_chunk_index_low + i, true);
    }

    // add the gradient heatmap scale
    d3.select("#chunks-yaxis").selectAll("svg").remove();

    var legend_g = d3.select("#chunks-yaxis").append("svg")
        .attr("height", chunk_graph_height)
        .selectAll()
        .data(colorScale.quantiles())
        .enter()
        .append("g");

    var elem_height = chunk_graph_height / colors.length;
    legend_g.append("rect")
        .attr("y", function(d, i) { return elem_height*i; })
        .attr("width", 10)
        .attr("height", elem_height)
        .style("fill", function(d, i) { return colors[i]})

    // now the text
    legend_g.append("text")
        .attr("x", 15)
        .attr("y", function(d, i) { return elem_height*i + elem_height/1.5; })
        .classed("legend-text", true)
        .text(function(d, i) { return bytesToStringNoDecimal(d)})
        .style("font-size", "10px");
}

function xMax() {
    // find the max TS value
    return memoro.filter_max_time();
}

function drawChunkXAxis() {

    var xAxis = d3.axisBottom(x)
        .tickFormat(function(xval) {
            if (max_x < 1000000000)
                return (xval / 1000).toFixed(1) + "Kc";
            else if (max_x < 10000000000) return (xval / 1000000).toFixed(1) + "Mc";
            else return (xval / 1000000000).toFixed(1) + "Gc";
        })
        .ticks(7)
        //.orient("bottom");

    var max_x = memoro.filter_max_time();
    var min_x = memoro.filter_min_time();

    x.domain([min_x, max_x]);

    d3.select("#chunk-x-axis").remove();

    d3.select("#chunk-axis")
        .append("svg")
        .attr("id", "chunk-x-axis")
        .attr("width", chunk_graph_width-chunk_y_axis_space)
        .attr("height", 30)
        .append("g")
        .attr("class", "axis")
        .call(xAxis);
}

function drawAggregateAxis() {

    var xAxis = d3.axisBottom(x)
        .tickFormat(function(xval) {
            if (max_x < 1000000000)
                return (xval / 1000).toFixed(1) + "Kc";
            else if (max_x < 10000000000) return (xval / 1000000).toFixed(1) + "Mc";
            else return (xval / 1000000000).toFixed(1) + "Gc";
        })
        //.orient("bottom")
        .ticks(7)
        .tickSizeInner(-120);

    var max_x = memoro.filter_max_time();
    var min_x = memoro.filter_min_time();

    x.domain([min_x, max_x]);

    d3.select("#aggregate-x-axis").remove();

    var xaxis_height = aggregate_graph_height*0.8 - 5;
    d3.select("#aggregate-group")
        .append("g")
        .attr("id", "aggregate-x-axis")
        //.attr("width", chunk_graph_width-chunk_y_axis_space)
        .attr("transform", "translate(0," + xaxis_height + ")")
        .attr("class", "axis")
        .call(xAxis);
}

function drawGlobalAggregateAxis() {

    var xAxis = d3.axisBottom(global_x)
        .tickFormat(function(xval) {
            if (max_x < 1000000000)
                return (xval / 1000).toFixed(1) + "Kc";
            else if (max_x < 10000000000) return (xval / 1000000).toFixed(1) + "Mc";
            else return (xval / 1000000000).toFixed(1) + "Gc";
        })
        .ticks(7)
        .tickSizeInner(-120);

    d3.select("#fg-aggregate-x-axis").remove();

    var xaxis_height = aggregate_graph_height*0.8 - 5;
    d3.select("#fg-aggregate-group")
        .append("g")
        .attr("id", "fg-aggregate-x-axis")
        .attr("transform", "translate(0," + xaxis_height + ")")
        .attr("class", "axis")
        .call(xAxis);
}

function drawGlobalAggregatePath() {

    y = d3.scaleLinear()
        .range([aggregate_graph_height*.8 - 10, 0]);

    var fg_width = window.innerWidth *0.75;
    global_x = d3.scaleLinear()
        .range([0, fg_width - chunk_y_axis_space]);

    // find the max TS value
    max_x = xMax();
    global_x.domain([0, max_x]);

    var aggregate_data = memoro.aggregate_all();

    aggregate_max = memoro.max_aggregate();
    var binned_ag = aggregate_data;

    y.domain(d3.extent(binned_ag, function(v) { return v["value"]; }));

    var yAxisRight = d3.axisRight(y)
        .ticks(5)
        .tickFormat(bytesToStringNoDecimal);

    var line = d3.line()
        .x(function(v) { return global_x(v["ts"]); })
        .y(function(v) { return y(v["value"]); })
        .curve(d3.curveStepAfter);

    var area = d3.area()
        .x(function(v) { return global_x(v["ts"]); })
        .y0(aggregate_graph_height*0.8 - 10)
        .y1(function(v) { return y(v["value"]); })
        .curve(d3.curveStepAfter);

    d3.select("#fg-aggregate-path").remove();
    d3.select("#fg-aggregate-y-axis").remove();

    var fg_aggregate_graph_g = d3.select("#fg-aggregate-group");
    fg_aggregate_graph_g.selectAll("rect").remove();
    fg_aggregate_graph_g.selectAll("g").remove();

    fg_aggregate_graph_g.append("rect")
        .attr("width", fg_width-chunk_y_axis_space + 2)
        .attr("height", aggregate_graph_height)
        .classed("aggregate-background", true);


    var yaxis_height = aggregate_graph_height;
    console.log("y axis height is " + aggregate_graph_height)

    fg_aggregate_graph_g.append("path")
        .datum(binned_ag)
        .attr("fill", "none")
        .attr("stroke", "steelblue")
        .attr("stroke-width", 1.5)
        .attr("transform", "translate(0, 5)")
        .attr("height", yaxis_height)
        .attr("id", "fg-aggregate-path")
        .attr("d", line);
    d3.select("#fg-aggregate-group").append("g")
        .attr("class", "y axis")
        .attr("transform", "translate(" + (fg_width - chunk_y_axis_space + 3) + ", 5)")
        .attr("height", yaxis_height)
        .attr("id", "fg-aggregate-y-axis")
        .style("fill", "white")
        .call(yAxisRight);

    fg_aggregate_graph_g.append("path")
        .datum(binned_ag)
        .classed("area", true)
        .attr("transform", "translate(0, 5)")
        .attr("id", "fg-aggregate-area")
        .attr("d", area);

    // draw the focus line on the graph
    var focus_g = fg_aggregate_graph_g.append("g")
        .classed("focus-line", true);

    fg_aggregate_graph_g.append("rect")
        .attr("width", fg_width-chunk_y_axis_space + 2)
        .attr("height", yaxis_height)
        .style("fill", "transparent")
        .on("mouseover", function() { focus_g.style("display", null); })
        .on("mouseout", function() { focus_g.style("display", "none"); })
        .on("mousemove", mousemove);

    focus_g.append("line")
        .attr("y0", 0).attr("y1", yaxis_height-5)
        .attr("x0", 0).attr("x1", 0);
    var text = focus_g.append("text")
        .attr("x", 20)
        .attr("y", "30");
    text.append("tspan")
        .classed("focus-text", true)
        .attr("id", "time");
    text.append("tspan")
        .classed("focus-text", true)
        .attr("id", "value");

    // display on mouseover
    function mousemove() {
        var x0 = global_x.invert(d3.mouse(this)[0]);
        var y_pos = d3.bisector(function(d) {
            return d["ts"];
        }).left(binned_ag, x0, 1);
        var y = binned_ag[y_pos-1]["value"];
        focus_g.attr("transform", "translate(" + global_x(x0) + ",0)");
        focus_g.select("#time").text(Math.round(x0) + "c " + bytesToString(y));
        if (d3.mouse(this)[0] > fg_width / 2)
            focus_g.select("text").attr("x", -170);
        else
            focus_g.select("text").attr("x", 20);
    }

}

function drawAggregatePath() {

    y = d3.scaleLinear()
        .range([aggregate_graph_height*.8 - 10, 0]);

    max_x = xMax();

    var aggregate_data = memoro.aggregate_all();

    aggregate_max = memoro.max_aggregate();
    var binned_ag = aggregate_data;

    y.domain(d3.extent(binned_ag, function(v) { return v["value"]; }));

    var yAxisRight = d3.axisRight(y)
        .ticks(5)
        .tickFormat(bytesToStringNoDecimal);

    var line = d3.line()
        .x(function(v) { return x(v["ts"]); })
        .y(function(v) { return y(v["value"]); })
        .curve(d3.curveStepAfter);

    var area = d3.area()
        .x(function(v) { return x(v["ts"]); })
        .y0(aggregate_graph_height*0.8 - 10)
        .y1(function(v) { return y(v["value"]); })
        .curve(d3.curveStepAfter);

    d3.select("#aggregate-path").remove();
    d3.select("#aggregate-y-axis").remove();

    var aggregate_graph_g = d3.select("#aggregate-group");
    aggregate_graph_g.selectAll("rect").remove();
    aggregate_graph_g.selectAll("g").remove();

    aggregate_graph_g.append("rect")
        .attr("width", chunk_graph_width-chunk_y_axis_space + 2)
        .attr("height", aggregate_graph_height)
        //.style("fill", "#353a41")
        .classed("aggregate-background", true);

    var yaxis_height = .8 * aggregate_graph_height;
    aggregate_graph_g.append("path")
        .datum(binned_ag)
        .attr("fill", "none")
        .attr("stroke", "steelblue")
        .attr("stroke-width", 1.5)
        .attr("transform", "translate(0, 5)")
        .attr("id", "aggregate-path")
        .attr("d", line);
    d3.select("#aggregate-group").append("g")
        .attr("class", "y axis")
        .attr("transform", "translate(" + (chunk_graph_width - chunk_y_axis_space + 3) + ", 5)")
        //.attr("height", yaxis_height)
        .attr("id", "aggregate-y-axis")
        .style("fill", "white")
        .call(yAxisRight);

    aggregate_graph_g.append("path")
        .datum(binned_ag)
        .classed("area", true)
        .attr("transform", "translate(0, 5)")
        .attr("id", "aggregate-area")
        .attr("d", area);

    // draw the focus line on the graph
    var focus_g = aggregate_graph_g.append("g")
        .classed("focus-line", true);

    aggregate_graph_g.append("rect")
        .attr("width", chunk_graph_width-chunk_y_axis_space + 2)
        .attr("height", yaxis_height)
        .style("fill", "transparent")
        .on("mouseover", function() { focus_g.style("display", null); })
        .on("mouseout", function() { focus_g.style("display", "none"); })
        .on("mousemove", mousemove);

    focus_g.append("line")
        .attr("y0", 0).attr("y1", yaxis_height-5)
        .attr("x0", 0).attr("x1", 0);
    var text = focus_g.append("text")
        .classed("focus-text", true)
        .attr("x", 20)
        .attr("y", "30");
    text.append("tspan")
        .attr("id", "time");
    text.append("tspan")
        .attr("id", "value");

    // display on mouseover
    function mousemove() {
        var x0 = x.invert(d3.mouse(this)[0]);
        var y_pos = d3.bisector(function(d) {
            return d["ts"];
        }).left(binned_ag, x0, 1);
        var y = binned_ag[y_pos-1]["value"];
        focus_g.attr("transform", "translate(" + x(x0) + ",0)");
        focus_g.select("#time").text(Math.round(x0) + "c " + bytesToString(y));
        if (d3.mouse(this)[0] > chunk_graph_width / 2)
            focus_g.select("text").attr("x", -170);
        else
            focus_g.select("text").attr("x", 20);
    }
}

// copied this color hashing generation stuff from the d3 flamegraph code
// API is not clearly documented wrt the flameGraph::color function
function generateHash(name) {
    // Return a vector (0.0->1.0) that is a hash of the input string.
    // The hash is computed to favor early characters over later ones, so
    // that strings with similar starts have similar vectors. Only the first
    // 6 characters are considered.
    var hash = 0, weight = 1, max_hash = 0, mod = 10, max_char = 6;
    if (name) {
        for (var i = 0; i < name.length; i++) {
            if (i > max_char) { break; }
            hash += weight * (name.charCodeAt(i) % mod);
            max_hash += weight * (mod - 1);
            weight *= 0.70;
        }
        if (max_hash > 0) { hash = hash / max_hash; }
    }
    return hash;
}

function colorHash(name) {
    // Return an rgb() color string that is a hash of the provided name,
    // and with a warm palette.
    var vector = 0;
    if (name) {
        var nameArr = name.split('`');
        if (nameArr.length > 1) {
            name = nameArr[nameArr.length -1]; // drop module name if present
        }
        name = name.split('(')[0]; // drop extra info
        vector = generateHash(name);
    }
    var r = 0 + Math.round(200 * vector);
    var g = 150 + Math.round(50 * (1 - vector));
    var b = 220 + Math.round(25 * (1 - vector));
    return "rgb(" + r + "," + g + "," + b + ")";
}

function name(d) {
    return d.data.n || d.data.name;
}

function colorMapper(maxValue) {
    return (d) => {
        // Return highlight/random color for trunk nodes
        if ("children" in d)
            return d.highlight ? "#E600E6" : colorHash(name(d));

        switch (current_fg_type) {
          case "peak_waste":
            var score = d.data.value / maxValue;
            var badness_col = Math.round(score * (badness_colors.length - 1));
            return badness_colors[badness_col];

          default:
            var mean = gmean([d.data.lifetime_score < 0.01 ? 0.01 : d.data.lifetime_score, d.data.usage_score < 0.01 ? 0.01 : d.data.usage_score,
                d.data.useful_lifetime_score < 0.01 ? 0.01 : d.data.useful_lifetime_score]);
            var badness_col = Math.round((1.0 - mean) * (badness_colors.length - 1));
            return badness_colors[badness_col];
        };
    }
}

function filterTree(tree) {

    if (filter_words.length === 0)
        return false;

    for (var w in filter_words) {
        if (tree.name.indexOf(filter_words[w]) !== -1) {
            // found, node has keyword so keep all descendants
            console.log("found " + filter_words[w] + " in " + tree.name);
            return false;
        }
    }

    if ('children' in tree) {
        for (var i = tree.children.length - 1; i >= 0; i--) {
            var value = tree.children[i].value;
            var remove = filterTree(tree.children[i]);
            if (remove) {
                console.log("deleting child");
                tree.value -= value;
                tree.children.splice(i, 1);
            }
        }
    }

    if ('children' in tree && tree.children.length === 0) {
        delete tree.children;
        return true;
    } else if (!('children' in tree))
        return true;
    else return false;
}

function leafAgg(node, acc, agg) {
  let isLeaf = !("children" in node);
  if (isLeaf)
    return agg(acc, node);
  else
    return node.children.reduce((_acc, _child) => leafAgg(_child, _acc, agg), acc);
}

function updateHideNode() {
    let hideIndex = tree.children.findIndex(n => n.name === "Hide");
    if (showHideNode) {
        if (hideIndex !== -1) return;

        tree.value += hideNode.value;
        tree.children.push(hideNode);
    } else {
        if (hideIndex === -1) return;

        tree.value -= hideNode.value;
        tree.children.splice(hideIndex, 1);
    }
}

function loadFlameGraph() {
    switch (current_fg_type) {
      case "bytes_time":
        memoro.stacktree_by_bytes(current_fg_time);
        break;
      case "bytes_total":
        memoro.stacktree_by_bytes_total(current_fg_time);
        break;
      case "peak_waste":
        memoro.stacktree_by_peak_waste();
        break;
      case "num_allocs":
        memoro.stacktree_by_numallocs();
        break;
      default:
    }

    tree = memoro.stacktree();

    // Keep track of the "Hide" node to toggle its visibility
    let hideIndex = tree.children.findIndex(n => n.name === "Hide");
    hideNode = (hideIndex !== -1) ? tree.children[hideIndex] : { name: "Hide", value: 0 };

    updateHideNode();

    // Remove trunks with no children
    for (var i = tree.children.length - 1; i >= 0; i--) {
        var child = tree.children[i];
        if (!("children" in child)) {
            tree.value -= child.value;
            tree.children.splice(i, 1);
        }
    }

    console.log(tree);
}

function drawFlameGraph() {
    filterTree(tree); // it just seems easier to filter this here ...
    d3.select("#flame-graph-div").html("");

    var fg_width = window.innerWidth *0.70; // getboundingclientrect isnt working i dont understand this crap
    var fgg = d3.flameGraph()
        .width(fg_width)
        .height(window.innerHeight*0.65)
        .cellHeight(17)
        .transitionDuration(400)
        .transitionEase(d3.easeCubic)
        .minFrameSize(0)
        .sort(true)
        //Example to sort in reverse order
        .sort(function(a,b){ return d3.descending(a.value, b.value);})
        .title("");

/*    fgg.onClick(function (d) {
        console.info("You clicked on frame "+ d.data.name);
    });*/

    var tip = d3.tip()
        .direction("s")
        .offset([8, 0])
        .attr('class', 'd3-flame-graph-tip')
        .html(function(d) {
            var ret = "";
            if (current_fg_type === "num_allocs")
                ret = d.data.name + ", NumAllocs: " + d.data.value;
            else
                ret = d.data.name + ", bytes: " + bytesToString(d.data.value, 2);

            if ('lifetime_score' in d.data) {
               ret += "</br>Lifetime: " + d.data.lifetime_score.toFixed(2) + " Usage: " + d.data.usage_score.toFixed(2)
                + " Useful Life: " + d.data.useful_lifetime_score.toFixed(2);
            }
            return ret;

        });

    let maxLeaf = leafAgg(tree, 0, (acc, leaf) => Math.max(acc, leaf.value));
    fgg.tooltip(tip);
    fgg.color(colorMapper(maxLeaf));

    var details = document.getElementById("flame-graph-details");

    fgg.details(details);

    d3.select("#flame-graph-div")
        .datum(tree)
        .call(fgg);
}

function tabSwitchClick() {

    // doing this because when you start on global tab,
    // stuff is hidden and i dont know how to get these values
    // when the detailed tab is hidden
    // fucking javascript
    if (chunk_graph_width === 0) {
        chunk_graph_width = d3.select("#chunks-container").node().getBoundingClientRect().width;
        chunk_graph_height = d3.select("#chunks").node().getBoundingClientRect().height;
        console.log("width is now " + chunk_graph_width);

        clearChunks();
        drawStackTraces();
        drawChunkXAxis();

        drawAggregatePath();
        drawAggregateAxis();
    }
}

function drawEverything() {

    d3.selectAll("g > *").remove();
    d3.selectAll("svg").remove();

    barHeight = 12;
    chunk_graph_width = window.innerWidth / 2;
    chunk_graph_height = d3.select("#chunks").node().getBoundingClientRect().height;
    console.log("chunk graph height " + chunk_graph_height + " width " + chunk_graph_width);
    chunk_y_axis_space = chunk_graph_width*0.13;  // ten percent should be enough

    x = d3.scaleLinear()
        .range([0, chunk_graph_width - chunk_y_axis_space]);

    // find the max TS value
    max_x = xMax();
    x.domain([0, max_x]);

    drawChunkXAxis();

    aggregate_graph_height = d3.select("#aggregate-graph").node().getBoundingClientRect().height;
    if (aggregate_graph_height === 0) {
        aggregate_graph_height = d3.select("#fg-aggregate-graph").node().getBoundingClientRect().height;
    }
    console.log("agg graph height is " + aggregate_graph_height);

    var aggregate_graph = d3.select("#aggregate-graph")
        .append("svg")
        .attr("width", chunk_graph_width)
        .attr("height", aggregate_graph_height-10);

    aggregate_graph.on("mousedown", function() {

        var p = d3.mouse(this);

        aggregate_graph.append( "rect")
            .attr("rx", 6)
            .attr("ry", 6)
            .attr("x", p[0])
            .attr("y", 0)
            .attr("height", aggregate_graph_height*.8 - 5)
            .attr("width", 0)
            .classed("selection", true)
            .attr("id", "select-rect")
    })
    .on( "mousemove", function() {
        var s = aggregate_graph.select("#select-rect");

        var y = d3.select(this).attr("height");
        if( !s.empty()) {
            var p = d3.mouse( this),
                d = {
                    x       : parseInt( s.attr( "x"), 0),
                    y       : parseInt( s.attr( "y"), 0),
                    width   : parseInt( s.attr( "width"), 10),
                    height  : y
                },
                move = {
                    x : p[0] - d.x,
                    y : p[1] - d.y
                };

            if( move.x < 1 || (move.x*2<d.width - chunk_y_axis_space)) {
                d.x = p[0];
                d.width -= move.x;
            } else {
                d.width = move.x;
            }

            s.attr("width", d.width);
            s.attr("x", d.x);
        }
    })
    .on( "mouseup", function() {
        // remove selection frame
        var selection = aggregate_graph.selectAll("#select-rect");
        var x1 = parseInt(selection.attr("x"));
        var x2 = x1 + parseInt(selection.attr("width"));
        var t1 = x.invert(x1);
        var t2 = x.invert(x2);
        selection.remove();
        // set time filter
        memoro.set_filter_minmax(t1, t2);

        // redraw
        clearChunks();

        drawStackTraces();

        drawAggregatePath();
        drawAggregateAxis();

        drawChunkXAxis();
        setGlobalInfo();

    });

    var aggregate_graph_g = aggregate_graph.append("g");
    aggregate_graph_g.attr("id", "aggregate-group");

    var trace = d3.select("#trace");
    trace.html("Select an allocation point under \"Heap Allocations\"");

    loadFlameGraph()
    drawFlameGraph();

    var fg_width = window.innerWidth *0.75; // getboundingclientrect isnt working i dont understand this crap

    var fg_aggregate_graph = d3.select("#fg-aggregate-graph")
        .append("svg")
        .attr("width", fg_width)
        .attr("height", aggregate_graph_height-10);
    var fg_aggregate_graph_g = fg_aggregate_graph.append("g");
    fg_aggregate_graph_g.attr("id", "fg-aggregate-group");

    fg_aggregate_graph.on("mouseup", function() {

        var p = d3.mouse(this);

        var time = global_x.invert(p[0]);
        current_fg_time = time;

        d3.select("#global-focus").remove();
        var focus_g = fg_aggregate_graph_g.append("g")
            .classed("focus-line", true)
            .attr("id", "global-focus");

        var xval = global_x(time);
        focus_g.append("line")
            .attr("y1", 0).attr("y2", aggregate_graph_height*0.8-5)
            .attr("x1", xval).attr("x2", xval);
        loadFlameGraph()
        drawFlameGraph();
    });

    drawAggregatePath();
    drawGlobalAggregatePath();
    drawGlobalAggregateAxis();
    drawAggregateAxis();

    sortTraces()
    drawStackTraces();

    setGlobalInfo();
}

function setGlobalInfo() {
    var info = memoro.global_info();
    num_traces = info.num_traces;
    total_chunks = info.num_chunks;

    var alloc_time = info.alloc_time;
    var time_total = info.total_time;
    var percent_alloc_time = 100.0 * alloc_time / time_total;

    // the \u03C3 is a sigma (variance)
    var html = "Total alloc points: " + formatBignum(num_traces) +
        "</br>Total Allocations: " + formatBignum(total_chunks) +
        "</br>Max Heap: " + bytesToString(info.aggregate_max) +
        "</br>Global alloc time: " + alloc_time +
        "</br>which is " + percent_alloc_time.toFixed(2) + "% of program time." +
        "</br>Avg Lifetime: " + info.lifetime_avg.toFixed(2) + " \u03C3 " + info.lifetime_var.toFixed(2) +
        "</br>Avg Usage: " + info.usage_avg.toFixed(2) + " \u03C3 " + info.usage_var.toFixed(2) +
        "</br>Avg Useful Life: " + info.useful_lifetime_avg.toFixed(2) + " \u03C3 " + info.useful_lifetime_var.toFixed(2);

    d3.select("#global-info").html(html);
    d3.select("#global-info-fg").html(html);
}
var current_filter = "trace";
function typeFilterClick() {
    current_filter = "type";
    document.getElementById("filter-select").innerHTML = "Type <span class=\"caret\"></span>";
    document.getElementById("filter-select-fg").innerHTML = "Type <span class=\"caret\"></span>";
}

function stackFilterClick() {
    console.log("stack filter click");
    current_filter = "trace";
    document.getElementById("filter-select").innerHTML = "Trace <span class=\"caret\"></span>";
    document.getElementById("filter-select-fg").innerHTML = "Type <span class=\"caret\"></span>";
}

function stackFilter() {
    showLoader();
    setTimeout(function() {
        var element = document.querySelector("#filter-form");
        if (element.value === "")
            element = document.querySelector("#filter-form-fg");
        var filterText = element.value;
        element.value = "";
        var filterWords = filterText.split(" ");

        for (var w in filterWords) {
            filter_words.push(filterWords[w]);
            console.log("setting filter " + filterWords[w]);
            memoro.set_trace_keyword(filterWords[w]);
        }
        clearChunks();
        drawStackTraces();
        drawChunkXAxis();

        drawAggregatePath();
        drawAggregateAxis();
        drawGlobalAggregatePath();
        drawGlobalAggregateAxis();
        drawFlameGraph();
        hideLoader();
        setGlobalInfo();
    }, 100);
}

function stackFilterResetClick() {
    //drawChunks();
    showLoader();
    filter_words = [];
    setTimeout(function() {
        memoro.trace_filter_reset();
        clearChunks();
        drawStackTraces();
        drawChunkXAxis();

        drawAggregatePath();
        drawAggregateAxis();
        drawGlobalAggregatePath();
        drawGlobalAggregateAxis();
        drawFlameGraph();
        hideLoader();
        setGlobalInfo();
    }, 100);
}

function typeFilter() {
    showLoader();
    setTimeout(function() {
        var element = document.querySelector("#filter-form");
        var filterText = element.value;
        element.value = "";
        var filterWords = filterText.split(" ");

        for (var w in filterWords) {
            console.log("setting filter " + filterWords[w]);
            memoro.set_type_keyword(filterWords[w]);
        }
        clearChunks();
        drawStackTraces();
        drawChunkXAxis();

        drawAggregatePath();
        drawAggregateAxis();
        drawGlobalAggregatePath();
        drawGlobalAggregateAxis();
        drawFlameGraph();
        hideLoader();
        setGlobalInfo();
    }, 100);
}

function typeFilterResetClick() {
    //drawChunks();
    showLoader();
    setTimeout(function() {
        memoro.type_filter_reset();
        clearChunks();
        drawStackTraces();
        drawChunkXAxis();

        drawAggregatePath();
        drawAggregateAxis();
        drawGlobalAggregatePath();
        drawGlobalAggregateAxis();
        drawFlameGraph();
        hideLoader();
        setGlobalInfo();
    }, 100);
}

function filterExecuteClick() {
    if (current_filter === "trace")
        stackFilter();
    else
        typeFilter();
}

function resetTimeClick() {
    showLoader();
    setTimeout(function() {
        memoro.filter_minmax_reset();
        clearChunks();
        drawStackTraces();
        drawChunkXAxis();

        drawAggregatePath();
        drawAggregateAxis();
        hideLoader();
        setGlobalInfo();
    }, 100);
}

function showFilterHelp() {
    showModal("Filter Trace/Type", "Dropdown to the left of the input box specifies whether you want to filter stack \F\
    traces or types by keyword. \
    Enter space separated keywords. Any trace not containing those keywords (or not matching the specified types) will be filtered out. </br>",
    "fa-info-circle");
}

function flameGraphHelp() {
    showModal("Flame Graph", "Several aggregate data can be displayed by the global flame graph. \
    First, the total number of allocations (#Allocations) of each allocation point in the code across total program lifetime. \
    Second, the flame graph can show bytes allocated by each allocation point at a specific point in time. Choose the \
    specific time point by clicking on the aggregate graph below.", "fa-info-circle")
}

function globalInfoHelp() {
    showModal("Global Info", "Globally applicable data. </br> \
    <b> Total alloc points</b>: Total number of allocation points in the profiled run. </br> \
    <b> Total Allocations</b>: Total number of allocations made over the program lifetime. </br> \
    <b> Max Heap</b>: Bytes allocated on the heap at it's maximum point in time. </br> \
    <b> Global Alloc time</b>: Approximate time spent in allocation functions. This tends to be overestimated due to instrumentation. </br> \
    </br>The following scores provide quantitative intuition into how good or bad allocations from particular points were in several categories. \
    1.0 is best, 0.0 is worst. See the documentation for details on how these are calculated. </br> \
    <b> Avg Lifetime</b>: The average lifetime score and variance of all allocation points. </br> \
    <b> Avg Usage</b>: The average usage score and variance of all allocation points. </br> \
    <b> Avg Useful Life</b>: The average useful life score and variance of all allocation points. </br>", "fa-info-circle")
}

function updateActiveFGType() {
  let typeFunctions = {
    "num_allocs": "fgAllocationsClick()",
    "bytes_time": "fgBytesTimeClick()",
    "bytes_total": "fgBytesTotalClick()",
    "peak_waste": "fgPeakWasteClick()",
  };

  d3.selectAll("#fg-btn a.btn")
    .classed("active", (_, i, ns) =>
      ns[i].getAttribute("onclick") === typeFunctions[current_fg_type]);
}

function setFlameGraphNumAllocs() {
    if (current_fg_type != "num_allocs") {
        current_fg_type = "num_allocs";
        updateActiveFGType();
        loadFlameGraph()
        drawFlameGraph();
    }
}

function setFlameGraphBytesTime() {
    if (current_fg_type != "bytes_time") {
        current_fg_type = "bytes_time";
        updateActiveFGType();
        loadFlameGraph()
        drawFlameGraph();
    }
}

function setFlameGraphBytesTotal() {
  if (current_fg_type != "bytes_total") {
      current_fg_type = "bytes_total";
      updateActiveFGType();
      loadFlameGraph()
      drawFlameGraph();
  }
}

function setFlameGraphPeakWaste() {
  if (current_fg_type != "peak_waste") {
      current_fg_type = "peak_waste";
      updateActiveFGType();
      loadFlameGraph()
      drawFlameGraph();
  }
}

function traceSort(pred) {
    current_sort_order = pred;
    sortTraces();
    drawStackTraces();
}

function toggleHideNodeVisibility() {
    showHideNode = !showHideNode;
    d3.select("#toggle-hide-btn")
      .classed("active", showHideNode);
    updateHideNode();

    drawFlameGraph();
}

// TODO separate out functionality, modularize
module.exports = {
    updateData: updateData,
    stackFilterClick: stackFilterClick,
    stackFilterResetClick: stackFilterResetClick,
    typeFilterClick: typeFilterClick,
    typeFilterResetClick: typeFilterResetClick,
    filterExecuteClick: filterExecuteClick,
    filterFgExecuteClick: filterExecuteClick,
    chunkScroll: chunkScroll,
    traceScroll: traceScroll,
    resetTimeClick: resetTimeClick,
    showFilterHelp: showFilterHelp,
    setFlameGraphBytesTime: setFlameGraphBytesTime,
    setFlameGraphBytesTotal: setFlameGraphBytesTotal,
    setFlameGraphNumAllocs: setFlameGraphNumAllocs,
    setFlameGraphPeakWaste: setFlameGraphPeakWaste,
    flameGraphHelp: flameGraphHelp,
    traceSort: traceSort,
    globalInfoHelp: globalInfoHelp,
    tabSwitchClick: tabSwitchClick,
    toggleHideNodeVisibility,
};
