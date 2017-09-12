var d3 = require("d3");
var autopsy = require('../cpp/build/Debug/autopsy.node');
var path = require('path')
var async = require('async');

function bytesToString(bytes,decimals) {
    if(bytes == 0) return '0 B';
    var k = 1024,
        dm = decimals || 2,
        sizes = ['B', 'KB', 'MB', 'GB', 'TB', 'PB', 'EB', 'ZB', 'YB'],
        i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + ' ' + sizes[i];
}

var bytesToStringNoDecimal = function (bytes) {
    // One way to write it, not the prettiest way to write it.

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
var chunk_y_axis_space;
var x;  // the x range
var max_x;
var num_traces;


var aggregate_max = 0;

function showLoader() {
    var element = document.querySelector("#loadspinner");
    element.style.visibility = "visible";
}

function hideLoader() {
    var element = document.querySelector("#loadspinner");
    element.style.visibility = "hidden";
}

// file open callback function
function updateData(datafile) {

    showLoader();

    var folder = path.dirname(datafile)

    console.log("running now");
    async.parallel([function(callback) {
        var res = autopsy.set_dataset(folder+'/');
        callback(null, res)

    }], function(err, results) {
        hideLoader();
        var result = results[0];
        if (!result.result) {
            $(".modal-title").text("Error");
            $(".modal-body").text("File parsing failed with error: " + result.message);
            $("#main-modal").modal("show")
        } else {

            // add default "main" filter?
            drawEverything();
        }
    })
/*    console.log(folder + "/")
    var result = autopsy.set_dataset(folder + "/");
    if (!result.result) {
        $(".modal-title").text("Error");
        $(".modal-body").text("File parsing failed with error: " + result.message);
        $("#main-modal").modal("show")
    } else {

        // add default "main" filter?
        drawEverything();
    }*/
    console.log("done!!");
}

function drawStackTraces() {

    var trace = d3.select("#trace")
    var traces_div = d3.select("#traces")
    traces_div.selectAll("svg").remove();

    //var max_x = xMax();
    var max_x = autopsy.filter_max_time();
    var min_x = autopsy.filter_min_time();

    x.domain([min_x, max_x]);

    var traces = autopsy.traces();
    num_traces = traces.length;
    total_chunks = 0;

    var cur_background_class = 0;
    traces.forEach(function (d, i) {
        //console.log("trace index is " + i);
        //if (!filterStackTrace(d["trace"])) return;

        total_chunks += d.num_chunks;
        var rectHeight = 55;
        var new_svg_g = traces_div
            .append("svg")
            .attr("width", chunk_graph_width)
            .attr("height", rectHeight)
            .classed("svg_spacing_trace", true)
            .on("mouseover", function(x) {
            })
            .on("click", function(x) {
                if (d3.select(this).classed("selected")) {
                    d3.selectAll(".select-rect").style("display", "none");
                    d3.select(this)
                        .classed("selected", false);
                    trace.html("");
                    clearChunks();
                } else {
                    trace.html(d.trace.replace(/\|/g, "</br><hr>"));
                    d3.selectAll(".select-rect").style("display", "none");
                    d3.select(this).selectAll(".select-rect").style("display", "inline");
                    d3.select(this)
                        .classed("selected", true);
                    drawChunks(d);
                }
            })
            .on("mouseout", function(x) {
            })
            .append("g");

        new_svg_g.append("rect")
            .attr("transform", "translate(0, 0)")
            .attr("width", chunk_graph_width - chunk_y_axis_space)
            .attr("height", rectHeight)
            .attr("class", function(x) {
                if (cur_background_class == 0) {
                    cur_background_class = 1;
                    return "background1";
                } else {
                    cur_background_class = 0;
                    return "background2";
                }

            })
            .on("mouseover", function(x) {
                //div.attr("width", width);
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

        sampled = autopsy.aggregate_trace(d.trace_index);
        var peak = d3.max(sampled, function (x) {
            return x["value"];
        });

        new_svg_g.append("text")
            .style("font-size", "small")
		    .style("fill", "white")
            .attr("x", 5)
            .attr("y", "15")
            .text("Chunks: " + (d.num_chunks) + ", Peak Bytes: " + bytesToString(peak));

        var y = d3.scale.linear()
            .range([rectHeight-25, 0]);

        y.domain(d3.extent(sampled, function(v) { return v["value"]; }));

        var yAxisRight = d3.svg.axis().scale(y)
            .orient("right")
            .tickFormat(bytesToStringNoDecimal)
            .ticks(2);

        //console.log(JSON.stringify(steps));
        var line = d3.svg.line()
            .x(function(v) {
                if (v.ts > max_x)
                    console.log("bigger than max x!! "+ v.ts);
                return x(v["ts"]);
            })
            .y(function(v) { return y(v["value"]); })
            .interpolate('step-after');

        new_svg_g.append("path")
            .datum(sampled)
            .attr("fill", "none")
            .attr("stroke", "#c8c8c8")
            .attr("stroke-linejoin", "round")
            .attr("stroke-linecap", "round")
            .attr("stroke-width", 1.5)
            .attr("transform", "translate(0, 20)")
            .attr("d", line);
        new_svg_g.append("g")
            .attr("class", "y axis")
            .attr("transform", "translate(" + (chunk_graph_width - chunk_y_axis_space + 1) + ",20)")
            .style("fill", "white")
            .call(yAxisRight);

    });

}

function tooltip(chunk) {
    return function() {
        var div = d3.select("#tooltip")
        div.transition()
            .duration(200)
            .style("opacity", .9);
        div .html(bytesToString(chunk["size"]) + "</br> Reads: " + chunk["num_reads"] + "</br>Writes: " + chunk["num_writes"])
            .style("left", (d3.event.pageX) + "px")
            .style("top", (d3.event.pageY - 28) + "px");
        //div.attr("width", width);
    }
}

function renderChunkSvg(chunk, text, bottom) {
    var min_x = autopsy.filter_min_time()
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
        .attr("width", Math.max(x(min_x + chunk["ts_end"] - chunk["ts_start"]), 2))
        .attr("height", barHeight)
        .classed("data_bars", true)
        .on("mouseover", tooltip(chunk))
        .on("mouseout", function(d) {
            div.transition()
                .duration(500)
                .style("opacity", 0);
        });
    new_svg_g.append("text")
        .attr("x", (chunk_graph_width - 130))
        .attr("y", 6)
        .text(text.toString())
        .classed("chunk_text", true)
}

function removeChunksTop(num) {
    var svgs = d3.select("#chunks").selectAll("div")[0];

    for (i = 0; i < num; i++)
    {
        svgs[i].remove();
    }
}

function removeChunksBottom(num) {

    var svgs = d3.select("#chunks").selectAll("div")[0];
    var end = svgs.length;

    for (i = end - num; i < end; i++)
    {
        console.log("removing " + i);
        svgs[i].remove();
    }
}

var load_threshold = 80;
var current_trace_index = null;
var current_chunk_index = 0;
var current_chunk_index_low = 0;

function chunkScroll() {
    var element = document.querySelector("#chunks");
    var percent = 100 * element.scrollTop / (element.scrollHeight - element.clientHeight);
    if (percent > load_threshold) {
        var chunks = autopsy.trace_chunks(current_trace_index, current_chunk_index, 25);
        if (chunks.length > 0) {
            // there are still some to display
            //console.log("cur chunk index is " + current_chunk_index);
            var to_append = chunks.length;
            var to_remove = to_append;
            // remove from top
            //console.log("adding " + to_append + " to bottom");
            for (var i = 0; i < to_append; i++) {
                renderChunkSvg(chunks[i], current_chunk_index + i, true);
            }
            current_chunk_index += to_append;
            current_chunk_index_low += to_append;

            removeChunksTop(to_remove);
        }
    } else if (percent < (100 - load_threshold)) {
        if (current_chunk_index_low > 0) {
            var chunks = autopsy.trace_chunks(current_trace_index, Math.max(current_chunk_index_low-25, 0), 25);
            if (chunks.length == 0)
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
}

// draw the first X chunks from this trace
function drawChunks(trace) {

    // test if this is already present in the div
    if (current_trace_index === trace.trace_index)
        return;

    current_trace_index = trace.trace_index;

    var chunk_div = d3.select("#chunks");

    chunk_div.selectAll("div").remove();

    // find the max TS value
/*    var max_x = xMax();

    x.domain([0, max_x]);*/
    var max_x = autopsy.filter_max_time();
    var min_x = autopsy.filter_min_time();
    console.log("min x " + min_x + " max x " + max_x)

    x.domain([min_x, max_x]);

    current_chunk_index_low = trace.chunk_index;
    current_chunk_index = Math.min(trace.num_chunks, 200);
    chunks  = autopsy.trace_chunks(trace.trace_index, current_chunk_index_low, current_chunk_index);

    for (var i = 0; i < chunks.length; i++) {
        renderChunkSvg(chunks[i], current_chunk_index_low + i, true);
    }

    // TODO redo first last access lines
/*        cur_height = 0;
        g.append("line")
            .filter(function(chunk) {
                return filterChunk(chunk);
            })
            .each(function(d) {
                d3.select(this).attr({
                    y1: cur_height*barHeight,
                    y2: cur_height * barHeight + barHeight - 1,
                    x1: "ts_last" in d ? x(d["ts_last"]) : 0,
                    x2: "ts_last" in d ? x(d["ts_last"]) : 0,
                    // don't display if its 0 (unknown) or is the empty chunk
                    display: !("ts_last" in d) || d["ts_last"] === 0 ? "none" : null
                }).style("stroke", "black");
                if ("ts_last" in d)
                    cur_height++;
            });*/

}

function xMax() {
    // find the max TS value
    return autopsy.filter_max_time();
}

function drawChunkXAxis() {

    var xAxis = d3.svg.axis()
        .scale(x)
        .tickFormat(function(xval) {
            if (max_x < 1000000000)
                return (xval / 1000) + "us";
            else return (xval / 1000000) + "ms";
        })
        .orient("bottom");

    var max_x = autopsy.filter_max_time();
    var min_x = autopsy.filter_min_time();

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

function drawAggregatePath() {

    var y = d3.scale.linear()
        .range([100, 0]);

    console.log("aggregate")
    max_x = xMax();

    var aggregate_data = autopsy.aggregate_all();

    aggregate_max = autopsy.max_aggregate();
    //var binned_ag = binAggregate(aggregate_data);
    var binned_ag = aggregate_data;

    y.domain(d3.extent(binned_ag, function(v) { return v["value"]; }));

    var yAxisRight = d3.svg.axis().scale(y)
        .orient("right").ticks(5)
        .tickFormat(bytesToStringNoDecimal);

    var line = d3.svg.line()
        .x(function(v) { return x(v["ts"]); })
        .y(function(v) { return y(v["value"]); })
        .interpolate('step-after');

    d3.select("#aggregate-path").remove();
    d3.select("#aggregate-y-axis").remove();

    var aggregate_graph_g = d3.select("#aggregate-group");
    aggregate_graph_g.selectAll("rect").remove();
    aggregate_graph_g.selectAll("g").remove();

    aggregate_graph_g.append("rect")
        .attr("width", chunk_graph_width-chunk_y_axis_space + 2)
        .attr("height", 120)
        .style("fill", "#6d6d6d");


    console.log("graphing line")
    //console.log(d3.select("#aggregate-group"));
    var path = aggregate_graph_g.append("path")
        .datum(binned_ag)
        .attr("fill", "none")
        .attr("stroke", "lightgrey")
        .attr("stroke-linejoin", "round")
        .attr("stroke-linecap", "round")
        .attr("stroke-width", 1.5)
        .attr("transform", "translate(0, 5)")
        .attr("id", "aggregate-path")
        .attr("d", line);
    d3.select("#aggregate-group").append("g")
        .attr("class", "y axis")
        .attr("transform", "translate(" + (chunk_graph_width - chunk_y_axis_space + 3) + ", 5)")
        .attr("height", 110)
        .attr("id", "aggregate-y-axis")
        .style("fill", "white")
        .call(yAxisRight);
    console.log("done graphing line")

    // draw the focus line on the graph
    var focus_g = aggregate_graph_g.append("g")
        .classed("focus-line", true);

    aggregate_graph_g.append("rect")
        .attr("width", chunk_graph_width-chunk_y_axis_space + 2)
        .attr("height", 120)
        .style("fill", "transparent")
        .on("mouseover", function() { focus_g.style("display", null); })
        .on("mouseout", function() { focus_g.style("display", "none"); })
        .on("mousemove", mousemove);

    focus_g.append("line")
        .attr("y0", 0).attr("y1", 120)
        .attr("x0", 0).attr("x1", 0);
    var text = focus_g.append("text")
        .style("stroke", "black")
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
        focus_g.select("#time").text(Math.round(x0) + "ns " + bytesToString(y));
        if (d3.mouse(this)[0] > chunk_graph_width / 2)
            focus_g.select("text").attr("x", -170);
        else
            focus_g.select("text").attr("x", 20);
    }
}

function drawEverything() {

    d3.selectAll("g > *").remove();
    d3.selectAll("svg").remove();

    barHeight = 8;
    chunk_graph_width = d3.select("#chunks-container").node().getBoundingClientRect().width;
    chunk_graph_height = d3.select("#chunks-container").node().getBoundingClientRect().height;
    console.log("width: " + chunk_graph_width);
    chunk_y_axis_space = chunk_graph_width*0.13;  // ten percent should be enough


    x = d3.scale.linear()
        .range([0, chunk_graph_width - chunk_y_axis_space]);

    // find the max TS value
    max_x = xMax();
    x.domain([0, max_x]);

    drawStackTraces();

    drawChunkXAxis();

    var aggregate_graph = d3.select("#aggregate-graph")
        .append("svg")
        .attr("width", chunk_graph_width)
        .attr("height", 120);

    aggregate_graph.on("mousedown", function() {

        var p = d3.mouse( this);

        var y = d3.select(this).attr("height");
        aggregate_graph.append( "rect")
            .attr({
                rx      : 6,
                ry      : 6,
                class   : "selection",
                x       : p[0],
                y       : 0,
                width   : 0,
                height  : y
            })
    })
    .on( "mousemove", function() {
        var s = aggregate_graph.select( "rect.selection");

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
                }
            ;

            if( move.x < 1 || (move.x*2<d.width - chunk_y_axis_space)) {
                d.x = p[0];
                d.width -= move.x;
            } else {
                d.width = move.x;
            }

            s.attr( d);

        }
    })
    .on( "mouseup", function() {
        // remove selection frame
        var selection = aggregate_graph.selectAll( "rect.selection");
        var x1 = parseInt(selection.attr("x"));
        var x2 = x1 + parseInt(selection.attr("width"));
        var t1 = x.invert(x1);
        var t2 = x.invert(x2);
        selection.remove();
        // set a new filter
        // TODO use autopsy lib filters
        autopsy.set_filter_minmax(t1, t2);

        // redraw
        clearChunks();

        console.log("drawing stacks")
        drawStackTraces();

        console.log("drawing aggregate")
        drawAggregatePath();

        console.log("drawing axis")
        drawChunkXAxis();

    });

    var aggregate_graph_g = aggregate_graph.append("g");
    aggregate_graph_g.attr("id", "aggregate-group");

    drawAggregatePath();

    var info = d3.select("#global-info");
    info.html("Total alloc points: " + num_traces +
        "</br>Total Allocations: " + total_chunks +
        "</br>Max Heap: " + bytesToString(aggregate_max));

}

function stackFilterClick() {
    var element = document.querySelector("#filter-form");
    console.log("text is " + element.value);
    var filterText = element.value;
    element.value = "";
    var filterWords = filterText.split(" ");

    for (var w in filterWords) {
        console.log("setting filter " + filterWords[w]);
        autopsy.set_trace_keyword(filterWords[w]);
    }
    showLoader();
    // redraw
    clearChunks();

    drawStackTraces();
    drawChunkXAxis();

    drawAggregatePath();
    hideLoader();
}

function stackFilterResetClick() {
    //drawChunks();
    showLoader();
    autopsy.trace_filter_reset();
    clearChunks();
    drawStackTraces();
    drawChunkXAxis();

    drawAggregatePath();
    hideLoader();
}

function resetTimeClick() {
    showLoader();
    autopsy.filter_minmax_reset();
    clearChunks();
    drawStackTraces();
    drawChunkXAxis();

    drawAggregatePath();
    hideLoader();
}

function showFilterHelp() {
    $(".modal-title").text("Filter Trace");
    $(".modal-body").text("Filter stack traces (allocation points) by keyword. \
    Enter space separated keywords. Any trace not containing those keywords will be filtered out.");
    $("#main-modal").modal("show")
}

module.exports = {
    updateData: updateData,
    stackFilterClick: stackFilterClick,
    stackFilterResetClick: stackFilterResetClick,
    chunkScroll: chunkScroll,
    resetTimeClick: resetTimeClick,
    showFilterHelp: showFilterHelp
};
