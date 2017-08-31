var d3 = require("d3");
var remote = require("electron").remote;
var fs = require('fs');

var bytesToString = function (bytes) {
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

var json_file = "";
var display_data;
var filtered_data;
var binary_path = "";

var barHeight;

var chunk_graph_width;
var chunk_graph_height;
var chunk_y_axis_space;
var top_spacing = 30;
var x;  // the x range

var filters = [];
var traceFilters = [];

// file open callback function
function updateData(datafile) {
    json_file = datafile;
    var data = JSON.parse(fs.readFileSync(datafile, 'utf-8'));
    console.log(JSON.stringify(data));
    binary_path = data["binary"];
    display_data = data["data"]
    filtered_data = data["data"]

    drawEverything();
}
 
function filterChunk(chunk) {
    if (!("ts_start" in chunk))
        return false;
    for (var f in filters) {
        if (!filters[f](chunk)) return false;
    }
    return true;
}

function filterStackTrace(trace) {
    console.log("filtering traces with " + Object.keys(traceFilters).length)
    if (Object.keys(traceFilters).length === 0) return true;
    for (var f in traceFilters) {
        if (traceFilters[f](trace)) return true;
    }
    return false;
}

function filteredChunkLength(chunks) {
    var count = 0;
    for (var c in chunks) {
        if (filterChunk(chunks[c]))
            count++;
    }
    return count;
}

function drawChunks() {

    var div = d3.select("#tooltip")
    var trace = d3.select("#trace")
    var chunk_div = d3.select("#chunks")
        /*.style("height", "" + chunk_graph_height + "px")
        .style("width", "" + (chunk_graph_width) + "px")
        .style("overflow", "auto")
        .style("padding-right", "17px");*/

    chunk_div.selectAll("svg").remove();

/*    var x = d3.scale.linear()
        .range([0, chunk_graph_width - chunk_y_axis_space]);*/


    // find the max TS value
    var x_max = xMax();

    x.domain([0, x_max]);

    var cur_height = 0;
    var cur_background_class = 0;

    filtered_data.forEach(function (d, i) {
        //console.log("adding svg");
        if (!filterStackTrace(d["trace"])) return;

        //console.log("length is " + d["chunks"].length);
        var rectHeight = filteredChunkLength(d["chunks"]) * barHeight;
        var rectHeightMin = 60;
        cur_height = 0;
        var new_svg_g = chunk_div
            .append("svg")
            .attr("width", chunk_graph_width)
            .attr("height", rectHeight)
            .classed("svg_spacing", true)
            .on("mouseover", function(x) {
                d3.select(this)
                    .transition()
                    .duration(500)
                    .attr("height", Math.max(rectHeight*2, rectHeightMin + rectHeight))
            })
            .on("click", function(x) {
                if (d3.select(this).classed("selected")) {
                    d3.select(this)
                        .classed("selected", false)
                } else {
                    d3.select(this)
                        .classed("selected", true)
                }
            })
            .on("mouseout", function(x) {
                if (!d3.select(this).classed("selected"))
                    d3.select(this)
                        .transition()
                        .duration(500)
                        .attr("height", rectHeight)
            })
            .append("g");

        new_svg_g.append("rect")
            .attr("transform", "translate(0, 0)")
            .attr("width", chunk_graph_width - chunk_y_axis_space)
            .attr("height", Math.max(rectHeight*2, rectHeightMin + rectHeight))
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
                trace.html(d["trace"].replace(/\|/g, "</br><hr>"))
                //div.attr("width", width);
            });

        //console.log(d["chunks"].toString());
        var g = new_svg_g.selectAll("rect.data")
            .data(d["chunks"])
            .enter();

        g.append("rect")
            .filter(function(chunk) {
                return filterChunk(chunk);
            })
            .attr("transform", function (chunk, i) {
                if ("ts_start" in chunk) {
                    //console.log("process chunk " + chunk["ts_start"] + " x: " + x(chunk["ts_start"]) );
                    var ret = "translate("+ x(chunk["ts_start"])  +"," + cur_height * barHeight + ")";
                    cur_height++;
                    return ret;
                }
                else {
                    //console.log("empty chunk");
                    return "translate(0, 0)";
                }
            })
            .attr("width", function (chunk) {
                if ("ts_end" in chunk)
                    return x(chunk["ts_end"] - chunk["ts_start"]);
                else return x(0)
            })
            .attr("height", function(chunk) {
                if ("ts_end" in chunk)
                    return barHeight - 1;
                else return 0
            })
            .attr("class", "data_bars")
            .on("mouseover", function(d) {
                div.transition()
                    .duration(200)
                    .style("opacity", .9);
                div .html(bytesToString(d["size"]) + "</br> Reads: " + d["reads"] + "</br>Writes: " + d["writes"])
                    .style("left", (d3.event.pageX) + "px")
                    .style("top", (d3.event.pageY - 28) + "px");
                //div.attr("width", width);
            })
            .on("mouseout", function(d) {
                div.transition()
                    .duration(500)
                    .style("opacity", 0);
            });

        cur_height = 0;
        g.append("line")
            .filter(function(chunk) {
                return filterChunk(chunk);
            })
            .each(function(d) {
                d3.select(this).attr({
                    y1: cur_height*barHeight,
                    y2: cur_height * barHeight + barHeight - 1,
                    x1: "ts_first" in d ? x(d["ts_first"]) : 0,
                    x2: "ts_first" in d ? x(d["ts_first"]) : 0,
                    // don't display if its 0 (unknown) or is the empty chunk
                    display: !("ts_first" in d) || d["ts_first"] === 0 ? "none" : null
                }).style("stroke", "black");
                if ("ts_first" in d)
                    cur_height++;
            });

        cur_height = 0;
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
            });

        var y = d3.scale.linear()
            .range([Math.max(rectHeight, rectHeightMin)-10, 0]);

        var starts = [];
        d["chunks"].forEach(function(chunk) {
            if ("ts_start" in chunk) {
                if (!filterChunk(chunk)) return;
                starts.push({"ts":chunk["ts_start"], "value":chunk["size"]});
                starts.push({"ts":chunk["ts_end"], "value":-chunk["size"]});
            }
        });
        starts.sort(function(a, b) { return a["ts"] - b["ts"]});
        var running = 0;
        var steps = [{"ts":0, "value":0}];
        starts.forEach(function (v) {
            running += v["value"];
            steps.push({"ts":v["ts"], "value":running})
        });
        steps.push({"ts":x_max, "value":steps[steps.length-1]["value"]});

        y.domain(d3.extent(steps, function(v) { return v["value"]; }));

        var yAxisRight = d3.svg.axis().scale(y)
            .orient("right")
            .tickFormat(bytesToString)
            .ticks(4);

        //console.log(JSON.stringify(steps));
        var line = d3.svg.line()
            .x(function(v) { return x(v["ts"]); })
            .y(function(v) { return y(v["value"]); })
            .interpolate('step-after');

        new_svg_g.append("path")
            .datum(steps)
            .attr("fill", "none")
            .attr("stroke", "black")
            .attr("stroke-linejoin", "round")
            .attr("stroke-linecap", "round")
            .attr("stroke-width", 1.5)
            .attr("transform", "translate(0, " + (rectHeight+5) + ")")
            .attr("d", line);
        new_svg_g.append("g")
            .attr("class", "y axis")
            .attr("transform", "translate(" + (chunk_graph_width - chunk_y_axis_space + 1) + "," + (rectHeight+5) + ")")
            .style("fill", "white")
            .call(yAxisRight);
    });
}

function xMax() {
    // find the max TS value
    return d3.max(filtered_data, function(d) {
        if ("trace" in d) {
            if (!filterStackTrace(d["trace"])) return 0;
            return d3.max(d["chunks"], function(chunk, i) {
                if ("ts_start" in chunk) {
                    for (var f in filters) {
                        if (!filters[f](chunk)) return 0;
                    }
                    return chunk["ts_end"]
                }
            })
        } else
            return 0
    });
}

function drawChunkXAxis() {
    /*x = d3.scale.linear()
        .range([0, chunk_graph_width - chunk_y_axis_space]);*/


    var xAxis = d3.svg.axis()
        .scale(x)
        .orient("bottom");

    var x_max = xMax();

    x.domain([0, x_max]);

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

function aggregateData() {
    var starts = [];
    var x_max = xMax();
    filtered_data.forEach(function(trace) {
        if (!filterStackTrace(trace["trace"])) return;
        trace["chunks"].forEach(function(chunk) {
            if ("ts_start" in chunk) {
                for (var f in filters) {
                    if (!filters[f](chunk)) return;
                }
                starts.push({"ts":chunk["ts_start"], "value":chunk["size"]});
                starts.push({"ts":chunk["ts_end"], "value":-chunk["size"]});
            }
        })
    });
    starts.sort(function(a, b) { return a["ts"] - b["ts"]});
    var running = 0;
    var steps = [{"ts":0, "value":0}];
    starts.forEach(function (v) {
        running += v["value"];
        steps.push({"ts":v["ts"], "value":running})
    });
    steps.push({"ts":x_max, "value":steps[steps.length-1]["value"]});

    return steps;
}

function drawAggregatePath() {

    var y = d3.scale.linear()
        .range([100, 0]);

    var aggregate_data = aggregateData();

    y.domain(d3.extent(aggregate_data, function(v) { return v["value"]; }));

    var yAxisRight = d3.svg.axis().scale(y)
        .orient("right").ticks(5)
        .tickFormat(bytesToString);

    var line = d3.svg.line()
        .x(function(v) { return x(v["ts"]); })
        .y(function(v) { return y(v["value"]); })
        .interpolate('step-after');

    d3.select("#aggregate-path").remove();
    d3.select("#aggregate-y-axis").remove();

    console.log(d3.select("#aggregate-group"));
    d3.select("#aggregate-group").append("path")
        .datum(aggregate_data)
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
}

function drawEverything() {

    d3.selectAll("g > *").remove();
    d3.selectAll("svg").remove();


    barHeight = 5;
    chunk_graph_width = d3.select("#chunks-container").node().getBoundingClientRect().width;
    chunk_graph_height = d3.select("#chunks-container").node().getBoundingClientRect().height;
    console.log("width: " + chunk_graph_width);
    chunk_y_axis_space = chunk_graph_width*0.12;  // ten percent should be enough

    x = d3.scale.linear()
        .range([0, chunk_graph_width - chunk_y_axis_space]);


    var xAxis = d3.svg.axis()
        .scale(x)
        .orient("bottom");

    var div = d3.select("#tooltip")
        .classed("tooltip", true);
    /*var trace = d3.select("#trace")
        .classed("trace", true)
        .style("left", "" + (chunk_graph_width + 5) + "px")
        .style("top", "40px");*/

/*    d3.select("#chunks-container")
        .style("height", "" + (win_size[1]-100)+ "px")
        .style("width", "" + (chunk_graph_width) + "px");*/

/*    var chunk_div = d3.select("#chunks")
        .style("height", "" + chunk_graph_height + "px")
        .style("width", "" + (chunk_graph_width) + "px")
        .style("overflow", "auto")
        .style("top", "40px")
        .style("padding-right", "17px");*/

    drawChunks();

    // find the max TS value
    var x_max = xMax();

    x.domain([0, x_max]);

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

            if( move.x < 1 || (move.x*2<d.width)) {
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
        filters["ts_filter"] = function(chunk) {
            if (!("ts_start" in chunk)) return false;
            var tc1 = parseInt(chunk["ts_start"]);
            var tc2 = parseInt(chunk["ts_end"]);
            return t1 <= tc2 && tc1 <= t2;
        };
        // redraw
        drawChunks();

        drawAggregatePath();

        drawChunkXAxis();

    })

    var aggregate_graph_g = aggregate_graph.append("g");
    aggregate_graph_g.attr("id", "aggregate-group");

    aggregate_graph_g.append("rect")
        .attr("width", chunk_graph_width-chunk_y_axis_space + 2)
        .attr("height", 120)
        .style("fill", "#6d6d6d");


    drawAggregatePath();

    function type(d) {
        d.value = +d.value; // coerce to number
        return d;
    }

}

function stackFilterClick() {
    var element = document.querySelector("#filter-form");
    console.log("text is " + element.value);
    var filterText = element.value;
    element.value = "";
    var filterWords = filterText.split(" ");
    for (var word in filterWords) {
        console.log("adding ST filter for word " + filterWords[word]);
        traceFilters[filterWords[word]] = function(trace) {
            console.log("filtering for " + word);
            var ret = trace.indexOf(filterWords[word]);
            console.log("returned " + ret);
            if (ret !== -1) return true;
            else return false;
        }
    }

    // redraw
    drawChunks();

    drawChunkXAxis();

    drawAggregatePath();

}

function stackFilterResetClick() {
    for (var f in traceFilters) {
        delete traceFilters[f];
    }
    drawChunks();

    drawChunkXAxis();

    drawAggregatePath();
}

module.exports = {
    updateData: updateData,
    stackFilterClick: stackFilterClick,
    stackFilterResetClick: stackFilterResetClick
};