
let wsocket;
let chartDaily;

function websocketSend(msg) {
    console.debug("websocket sending", msg)
    wsocket.send(msg);
}

function setSimSpeed(val) {
    $("#simulator-speed").val(val);
}

function setSimTimeField(val) {
    if (val) {
        let d = new Date(val * 1000);
        $("#simulator-time").html(d.toUTCString());
        $("#simulator-unixtime").html(val);
    } else {
        $("#simulator-time").html("-");
        $("#simulator-unixtime").html("-");
    }
}

function setSimStatus(status) {
    $("#simulator-status").html(status.toUpperCase());
}

function setWebsocketStatus(code) {
    let status = function() {
        switch (wsocket.readyState) {
            case 0 : return "CONNECTING";
            case 1 : return "OPEN";
            case 2 : return "CLOSING";
            case 3 : return "CLOSED";
            default: return "UNKNOWN";
        }
    }();
    $("#websocket-status").html(status);
}

function onKpiDailyReceived(data) {
    chartDaily.series[0].addPoint([data.time * 1000, data.value]);
}

function onKpiWeeklyReceived(data) {
    chartDaily.series[1].addPoint([data.time * 1000, data.value]);
}

function websocketConnect() {

    if (wsocket && (wsocket.readyState === 0 || wsocket.readyState === 1)) {
        console.log("Websocket already connected or connecting in progress");
        return;
    }

    let host = "";
    if (location.protocol === "https:") {
        host = "wss://";
    } else {
        host = "ws://";
    }
    host += location.host + "/wsapi";

    try {
        wsocket = new WebSocket(host);

        wsocket.onopen = function (d) {
            setWebsocketStatus("connected");
            // console.log("websocket open", d);

            //websocketSend(JSON.stringify({
            //    hello: "backend"
            //}));
        };

        wsocket.onmessage = function (msg) {
            console.log("websocket message received ", msg.data);

            let data = JSON.parse(msg.data);
            if (data.sim_time) {
                setSimTimeField(data.sim_time);
            }
            if (data.sim_status) {
                setSimStatus(data.sim_status);
            }
            if (data.sim_speed) {
                setSimSpeed(data.sim_speed);
            }
            if (data.kpi_daily) {
                onKpiDailyReceived(data.kpi_daily);
            }
            if (data.kpi_weekly) {
                onKpiWeeklyReceived(data.kpi_weekly);
            }
            if (data.operation_statistics) {
                $("#statistics-view").html(JSON.stringify(data.operation_statistics, null, "  "));
            }
        };

        wsocket.onclose = function (event) {
            setWebsocketStatus("closed - code " + event.code);
            setSimStatus("-");
            //console.log("websocket close - code ", event.code, event);
            setTimeout(websocketConnect, 2000);
        };

        wsocket.onerror = function(event) {
            setWebsocketStatus("error - code " + event.code);
            //console.error("Websocket error ", event);
        }

    } catch (exception) {
        console.error(exception);
    }
}

$("#button-start").click(function() {
    chartDaily.series[0].setData([]);
    chartDaily.series[1].setData([]);
    websocketSend(JSON.stringify({
        command: {
            type: "start"
        }
    }));
});

$("#button-stop").click(function() {
    websocketSend(JSON.stringify({
        command: {
            type: "stop"
        }
    }));
});

$("#button-pause").click(function() {
    websocketSend(JSON.stringify({
        command: {
            type: "pause"
        }
    }));
});

$("#button-resume").click(function() {
    websocketSend(JSON.stringify({
        command: {
            type: "resume"
        }
    }));
});

$("#simulator-speed").on("change", function() {
    websocketSend(JSON.stringify({
        command: {
            type: "speed",
            value: parseInt($(this).val())
        }
    }));
})

$("#button-get-statistics").click(function() {
    websocketSend(JSON.stringify({
        command: {
            type: "get_statistics"
        }
    }));
});

function resetStatistics() {
    websocketSend(JSON.stringify({
        command: {
            type: "reset_statistics"
        }
    }));
}

function getStatistics() {
    websocketSend(JSON.stringify({
        command: {
            type: "get_statistics"
        }
    }));
}

function setGlobalLoggingLevel(level) {
    websocketSend(JSON.stringify({
        command: {
            type: "global_logging_level",
            level: level
        }
    }));
}

function setChannelLoggingLevel(channel, level) {
    websocketSend(JSON.stringify({
        command: {
            type: "channel_logging_level",
            channel: channel,
            level: level
        }
    }));
}


$( document ).ready(function() {

    chartDaily = Highcharts.chart('container', {
        chart: {
            type: 'column'
        },
        title: {
            text: ''
        },
        xAxis: {
            type: 'datetime'
        },
        yAxis: {
            title: {
                text: 'KPI (kWh/kg)'
            }
        },
        time: {
            useUTC: true
        },

        series: [{
            data: [],
            lineWidth: 0.5,
            name: 'KPI daily'
        },{
            data: [],
            lineWidth: 0.5,
            name: 'KPI weekly'
        }]
    });

    websocketConnect();
});

