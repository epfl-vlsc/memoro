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
var aggregate_graph_height;
var chunk_y_axis_space;
var x;  // the x range
var y;  // aggregate y range
var max_x;
var num_traces;

var aggregate_max = 0;

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

// chunk color gradient scale
var colorScale = d3.scaleQuantile()
/*    .range(["#ffffff", "#f0f5f9", "#e0eaf3", "#d1e0ec", "#c1d5e6", "#b2cbe0", "#a2c1da", "#93b6d3", "#84accd", "#74a1c7",
        "#6597c1", "#558cba", "#4682b4"]);*/
.range(colors);

function showLoader() {
    var div = document.createElement("div");
    div.id = "loadspinner";
    div.className = "spinny-load";
    div.html = "Loading ...";
    document.getElementById("middle-column").appendChild(div);
}

function hideLoader() {
    var element = document.getElementById("loadspinner");
    element.parentNode.removeChild(element);
}

// file open callback function
function updateData(datafile) {

    showLoader();

    setTimeout(function() {
        var folder = path.dirname(datafile);

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

        var element = document.querySelector("#overlay");
        element.style.visibility = "hidden";
    }, 1000);

}

function constructInferences(inef) {
    var ret = "";
    if (inef.unused)
        ret += "-> unused chunks </br>"
    if (!inef.unused) {
        if (inef.read_only)
            ret += "-> read-only chunks </br>"
        if (inef.write_only)
            ret += "-> write-only chunks </br>"
        if (inef.short_lifetime)
            ret += "-> short lifetime chunks </br>"
        if (!inef.short_lifetime) {
            if (inef.late_free)
                ret += "-> chunks freed late </br>"
            if (inef.early_alloc)
                ret += "-> chunks allocated early </br>"
        }
        if (inef.increasing_allocs)
            ret += "-> increasing allocations </br>"
        if (inef.top_percentile)
            ret += "-> in top percentile of chunks allocated </br>"
    }

    return ret;
}

function drawStackTraces() {

    var trace = d3.select("#trace")
    var traces_div = d3.select("#traces")
    traces_div.selectAll("svg").remove();

    var info = d3.select("#inferences");
    info.html("")

    //var max_x = xMax();
    var max_x = autopsy.filter_max_time();
    var min_x = autopsy.filter_min_time();

    x.domain([min_x, max_x]);

    var traces = autopsy.traces();
    num_traces = traces.length;
    total_chunks = 0;

    var cur_background_class = 0;
    traces.forEach(function (d, i) {

        sampled = autopsy.aggregate_trace(d.trace_index);
        var peak = d3.max(sampled, function (x) {
            return x["value"];
        });
        total_chunks += d.num_chunks;
        var rectHeight = 55;

        var new_svg_g = traces_div
            .append("svg")
            .attr("width", chunk_graph_width)
            .attr("height", rectHeight)
            .classed("svg_spacing_trace", true)
            .on("mouseover", function(x) {
            })
            .on("click", function(s) {
                if (d3.select(this).classed("selected")) {
                    d3.selectAll(".select-rect").style("display", "none");
                    d3.select(this)
                        .classed("selected", false);
                    trace.html("Select an allocation point under \"Heap Allocation\"");
                    clearChunks();
                    var info = d3.select("#inferences");
                    info.html("");
                    d3.select("#stack-agg-path").remove();
                } else {
                    colorScale.domain([1, peak]);
                    trace.html(d.trace.replace(/\|/g, "</br><hr>"));
                    d3.selectAll(".select-rect").style("display", "none");
                    d3.select(this).selectAll(".select-rect").style("display", "inline");
                    d3.select(this)
                        .classed("selected", true);
                    drawChunks(d);
                    var info = d3.select("#inferences");
                    var inef = autopsy.inefficiencies(d.trace_index);
                    var html = constructInferences(inef);
                    info.html(html);

                    fresh_sampled = autopsy.aggregate_trace(d.trace_index);
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
                        .attr("stroke", "#c8c8c8")
                        .attr("stroke-width", 1.0)
                        .attr("transform", "translate(0, 5)")
                        .attr("d", agg_line);
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

        //sampled = autopsy.aggregate_trace(d.trace_index);

        new_svg_g.append("text")
            .style("font-size", "small")
		    .style("fill", "white")
            .attr("x", 5)
            .attr("y", "15")
            .text("Chunks: " + (d.num_chunks) + ", Peak Bytes: " + bytesToString(peak));

        var stack_y = d3.scaleLinear()
            .range([rectHeight-25, 0]);

        stack_y.domain(d3.extent(sampled, function(v) { return v["value"]; }));

        var yAxisRight = d3.axisRight(stack_y)
            //.orient("right")
            .tickFormat(bytesToStringNoDecimal)
            .ticks(2);

        //console.log(JSON.stringify(steps));
        var line = d3.line()
            .x(function(v) {
                return x(v["ts"]);
            })
            .y(function(v) { return stack_y(v["value"]); })
            .curve(d3.curveStepAfter);

        new_svg_g.append("path")
            .datum(sampled)
            .attr("fill", "none")
            .attr("stroke", "#c8c8c8")
            //.attr("stroke-linejoin", "round")
            //.attr("stroke-linecap", "round")
            .attr("stroke-width", 1.0)
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
        div .html(bytesToString(chunk["size"]) + "</br> Reads: " + chunk["num_reads"]
            + "</br>Writes: " + chunk["num_writes"] + "</br>")
            .style("left", (d3.event.pageX) + "px")
            .style("top", (d3.event.pageY - 28) + "px");
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
        .attr("width", Math.max(x(min_x + chunk["ts_end"] - chunk["ts_start"]), 3))
        .attr("height", barHeight)
        .style("fill", colorScale(chunk.size))
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
        .classed("chunk_text", true);

    new_svg_g.append("line")
        .attr("y1", 0)
        .attr("y2", barHeight)
        .attr("x1", x(chunk["ts_first"]))
        .attr("x2", x(chunk["ts_first"]))
        .style("stroke", "#65DC4C")
        .attr("display", chunk["ts_first"] === 0 ? "none" : null)
        .classed("firstlastbars", true);

    var next = 0;
    if (x(chunk["ts_last"]) - x(chunk["ts_first"]) < 1)
    {
        next = x(chunk["ts_first"]) + 2;
    } else {
        next = x(chunk["ts_last"]);
    }
    new_svg_g.append("line")
        .attr("y1", 0)
        .attr("y2", barHeight)
        .attr("x1", next)
        .attr("x2", next)
        .attr("display", chunk["ts_first"] === 0 ? "none" : null)
        .style("stroke", "#65DC4C")
        .classed("firstlastbars", true);
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
        svgs[i].remove();
    }
}

var load_threshold = 80;
var current_trace_index = null;
var current_chunk_index = 0;
var current_chunk_index_low = 0;

// TODO there is still a small bug in this where upscroll results in
// negative numbers ... somehow
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
    current_trace_index = null;
}

// draw the first X chunks from this trace
function drawChunks(trace) {

    // test if this is already present in the div
    if (current_trace_index === trace.trace_index)
        return;

    current_trace_index = trace.trace_index;

    var chunk_div = d3.select("#chunks");

    chunk_div.selectAll("div").remove();

    var max_x = autopsy.filter_max_time();
    var min_x = autopsy.filter_min_time();
    //console.log("min x " + min_x + " max x " + max_x)

    x.domain([min_x, max_x]);

    current_chunk_index_low = trace.chunk_index;
    current_chunk_index = Math.min(trace.num_chunks, 200);
    chunks  = autopsy.trace_chunks(trace.trace_index, current_chunk_index_low, current_chunk_index);

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
        .style("fill", "#ffffff")
        .text(function(d, i) { return bytesToStringNoDecimal(d)})
        .style("font-size", "10px");
}

function xMax() {
    // find the max TS value
    return autopsy.filter_max_time();
}

function drawChunkXAxis() {

    var xAxis = d3.axisBottom(x)
        .tickFormat(function(xval) {
            if (max_x < 1000000000)
                return (xval / 1000).toFixed(1) + "us";
            else return (xval / 1000000).toFixed(1) + "ms";
        })
        .ticks(10)
        //.orient("bottom");

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

function drawAggregateAxis() {

    var xAxis = d3.axisBottom(x)
        .tickFormat(function(xval) {
            if (max_x < 1000000000)
                return (xval / 1000).toFixed(1) + "us";
            else return (xval / 1000000).toFixed(1) + "ms";
        })
        //.orient("bottom")
        .ticks(10)
        .tickSizeInner(-120)

    var max_x = autopsy.filter_max_time();
    var min_x = autopsy.filter_min_time();

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

function drawAggregatePath() {

    y = d3.scaleLinear()
        .range([100, 0]);

    console.log("aggregate")
    max_x = xMax();

    var aggregate_data = autopsy.aggregate_all();

    aggregate_max = autopsy.max_aggregate();
    //var binned_ag = binAggregate(aggregate_data);
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
        .style("fill", "#353a41");


    var yaxis_height = .8 * aggregate_graph_height;
    console.log("graphing line")
    //console.log(d3.select("#aggregate-group"));
    var path = aggregate_graph_g.append("path")
        .datum(binned_ag)
        .attr("fill", "none")
        .attr("stroke", "steelblue")
        .attr("stroke-linejoin", "round")
        .attr("stroke-linecap", "round")
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
    console.log("done graphing line")

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
        .style("fill", "lightgray")
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
    chunk_graph_height = d3.select("#chunks").node().getBoundingClientRect().height;
    console.log("width: " + chunk_graph_width);
    chunk_y_axis_space = chunk_graph_width*0.13;  // ten percent should be enough


    x = d3.scaleLinear()
        .range([0, chunk_graph_width - chunk_y_axis_space]);

    // find the max TS value
    max_x = xMax();
    x.domain([0, max_x]);


    drawChunkXAxis();

    aggregate_graph_height = d3.select("#aggregate-graph").node().getBoundingClientRect().height;
    console.log("agg graph height is " + aggregate_graph_height)
    var aggregate_graph = d3.select("#aggregate-graph")
        .append("svg")
        .attr("width", chunk_graph_width)
        .attr("height", aggregate_graph_height-10);

    aggregate_graph.on("mousedown", function() {

        var p = d3.mouse(this);

        var y = d3.select(this).attr("height");
        aggregate_graph.append( "rect")
            .attr("rx", 6)
            .attr("ry", 6)
            .attr("x", p[0])
            .attr("y", 0)
            .attr("height", 110)
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
        // set a new filter
        // TODO use autopsy lib filters
        autopsy.set_filter_minmax(t1, t2);

        // redraw
        clearChunks();

        console.log("drawing stacks")
        drawStackTraces();

        console.log("drawing aggregate")
        drawAggregatePath();
        drawAggregateAxis();

        console.log("drawing axis")
        drawChunkXAxis();

    });

    var aggregate_graph_g = aggregate_graph.append("g");
    aggregate_graph_g.attr("id", "aggregate-group");

    drawAggregatePath();
    drawAggregateAxis();

    drawStackTraces();

    var trace = d3.select("#trace");
    trace.html("Select an allocation point number \"Heap Allocations\"")

    var alloc_time = autopsy.global_alloc_time();
    var time_total = autopsy.max_time() - autopsy.min_time();
    var percent_alloc_time = 100.0 * alloc_time / time_total;
    var info = d3.select("#global-info");
    info.html("Total alloc points: " + num_traces +
        "</br>Total Allocations: " + total_chunks +
        "</br>Max Heap: " + bytesToString(aggregate_max) +
        "</br>Global alloc time: " + alloc_time +
        "</br>which is " + percent_alloc_time.toFixed(2) + "% of program time.");

}

function stackFilterClick() {
    showLoader();
    setTimeout(function() {
        var element = document.querySelector("#filter-form");
        console.log("text is " + element.value);
        var filterText = element.value;
        element.value = "";
        var filterWords = filterText.split(" ");

        for (var w in filterWords) {
            console.log("setting filter " + filterWords[w]);
            autopsy.set_trace_keyword(filterWords[w]);
        }
        clearChunks();
        drawStackTraces();
        drawChunkXAxis();

        drawAggregatePath();
        drawAggregateAxis();
        hideLoader();
    }, 100);
}

function stackFilterResetClick() {
    //drawChunks();
    showLoader();
    setTimeout(function() {
        autopsy.trace_filter_reset();
        clearChunks();
        drawStackTraces();
        drawChunkXAxis();

        drawAggregatePath();
        drawAggregateAxis();
        hideLoader();
    }, 100);
}

function resetTimeClick() {
    showLoader();
    setTimeout(function() {
        autopsy.filter_minmax_reset();
        clearChunks();
        drawStackTraces();
        drawChunkXAxis();

        drawAggregatePath();
        drawAggregateAxis();
        hideLoader();
    }, 100);
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
