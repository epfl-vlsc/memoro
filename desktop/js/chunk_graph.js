var d3 = require("d3");
var remote = require("electron").remote;

function drawChunks() {
    d3.selection.prototype.moveToFront = function() {
        return this.each(function(){
            this.parentNode.appendChild(this);
        });
    };

    var win_size = remote.getCurrentWindow().getSize();

    var width = win_size[0],
        barHeight = 5;
    var chunk_graph_width = width/3 + 100;
    var chunk_graph_height = win_size[1]*0.66;

    var x = d3.scale.linear()
        .range([0, chunk_graph_width]);


    var xAxis = d3.svg.axis()
        .scale(x)
        .orient("bottom");

    var div = d3.select("#tooltip")
        .classed("tooltip", true);
    var trace = d3.select("#trace")
        .classed("trace", true)
        .style("left", "" + (chunk_graph_width + 50) + "px")
        .style("top", "5px");

    d3.select("#chunks-container")
        .style("height", "" + win_size[1]*0.66 + "px")
        .style("width", "" + (chunk_graph_width+101) + "px");

    var chunk_div = d3.select("#chunks")
        .style("height", "" + chunk_graph_height + "px")
        .style("width", "" + (chunk_graph_width+101) + "px")
        .style("overflow", "auto")
        .style("padding-right", "17px");

    d3.json("hplgst.json", function(error, data) {

        // find the max TS value
        var x_max = d3.max(data, function(d) {
            if ("trace" in d) {
                return d3.max(d["chunks"], function(chunk, i) {
                    if ("ts_start" in chunk)
                        return chunk["ts_end"]
                })
            } else
                return 0
        });

        x.domain([0, x_max]);

        var total_chunks = 0;
        data.forEach(function(d) {
            if ("trace" in d) {
                if ("chunks" in d)
                    total_chunks += d["chunks"].length - 1
            }
        });
        console.log("total chunk " + total_chunks);

        var cur_height = 0;
        var cur_background_class = 0;
        data.forEach(function (d) {
            //console.log("adding svg");

            var rectHeight = (d["chunks"].length-1) * barHeight;
            var rectHeightMin = 60;
            cur_height = 0;
            var new_svg_g = chunk_div
                .append("svg")
                .attr("width", chunk_graph_width + 50)
                .attr("height", rectHeight)
                .classed("svg_spacing", true)
                .on("mouseover", function(x) {
                    d3.select(this)
                        .transition()
                        .duration(500)
                        .attr("height", Math.max(rectHeight*2, rectHeightMin + rectHeight))
                })
                .on("mouseout", function(x) {
                    d3.select(this)
                        .transition()
                        .duration(500)
                        .attr("height", rectHeight)
                })
                .append("g");

            new_svg_g.append("rect")
                .attr("transform", "translate(0, 0)")
                .attr("width", chunk_graph_width)
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
                    trace.html(d["trace"].replace(/\|/g, "</br>"))
                    //div.attr("width", width);
                });

            //console.log(d["chunks"].toString());
            var g = new_svg_g.selectAll("rect.data")
                .data(d["chunks"])
                .enter();

            g.append("rect")
                .attr("transform", function (chunk, i) {
                    if ("ts_start" in chunk) {
                        //console.log("process chunk " + chunk["ts_start"]);
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
                    div .html("Bytes " + d["size"])
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
                .orient("right").ticks(5);

            console.log(JSON.stringify(steps));
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
                .attr("transform", "translate(" + chunk_graph_width + "," + (rectHeight+5) + ")")
                .style("fill", "red")
                .call(yAxisRight);
        });

        d3.select("#chunk-axis")
            .append("svg")
            .attr("width", chunk_graph_width)
            .attr("height", 30)
            .append("g")
            .attr("class", "axis")
            .call(xAxis);

        var y = d3.scale.linear()
            .range([100, 0]);

        var starts = [];
        data.forEach(function(trace) {
            trace["chunks"].forEach(function(chunk) {
                if ("ts_start" in chunk) {
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

        console.log(JSON.stringify(steps));
        y.domain(d3.extent(steps, function(v) { return v["value"]; }));

        var yAxisRight = d3.svg.axis().scale(y)
            .orient("right").ticks(5);

        var line = d3.svg.line()
            .x(function(v) { return x(v["ts"]); })
            .y(function(v) { return y(v["value"]); })
            .interpolate('step-after');

        var aggregate_graph = d3.select("#aggregate-graph")
            .append("svg")
            .attr("width", chunk_graph_width+60)
            .attr("height", 110)
            .append("g");

        aggregate_graph.append("path")
            .datum(steps)
            .attr("fill", "none")
            .attr("stroke", "black")
            .attr("stroke-linejoin", "round")
            .attr("stroke-linecap", "round")
            .attr("stroke-width", 1.5)
            .attr("d", line);
        aggregate_graph.append("g")
            .attr("class", "y axis")
            .attr("transform", "translate(" + chunk_graph_width + ", 0)")
            .style("fill", "red")
            .call(yAxisRight);

    });

    function type(d) {
        d.value = +d.value; // coerce to number
        return d;
    }

}
