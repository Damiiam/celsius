//--------------------------CONNECTION----------------------------------

var connection = new WebSocket('ws://'+location.hostname+':81/', ['arduino']);

    var play = false;

connection.onopen = () => {
    console.log('Conectado  -  ' + new Date());
}

connection.onmessage = (event) => {
    var obj = JSON.parse(event.data);
    if (obj.hh !== undefined){
        updateTime(obj.hh, obj.mm, obj.ss);
    }else if (obj.tempIC !== undefined){
        console.log(obj);
        plotGraphics(chart_ef, obj.tempIC);
        plotGraphics(chart_sf, obj.tempOC);
        plotGraphics(chart_ec, obj.tempIH);
        plotGraphics(chart_sc, obj.tempOH);
    }else if(obj.play !== undefined){
        play = obj.play;
        changeState();
    }else if(obj.samples !== undefined){
        document.getElementById("tx-samples").value = obj.samples;
    }else if(obj.alert !== undefined){
        alertResponse(obj.alert, 2);
    }else if(obj.ip !== undefined){
        informIp(obj.ip);
    }else if(obj.download !== undefined){
        updateDownload(obj);
    }
}

connection.onerror = (error) => {
  console.log('WebSocket Error!!!', error);
}

connection.onclose = () => {
    alertResponse('Conexión perdida. Refresca tu página', 5);
 };

function updateTime(h, m, s){
    var t = `${twoDigits(h)}:${twoDigits(m)}:${twoDigits(s)}`;
    document.getElementById("tx-timer").value = t;
}

function updateDownload(o) {
    if(o.download){
        enabledDownload(o.path);
    }else{
        disabledDownload();
    }
}

function informIp(ip){
    var btRun = document.getElementById("bt-run");
    btRun.setAttribute("title", `Servidor corriendo en la dirección ${ip}`);
}

//----------------------------STATE-------------------------------------

var hour, minute, second;

function changeState(){
    var timer = document.getElementById("tx-timer");
    var samples = document.getElementById("tx-samples");
    var btCentro = document.getElementById("bt-center");
    var btRun = document.getElementById("bt-run");
    if(play){
        btRun.style.backgroundColor = "#e85858";
        btCentro.className = "bt-cuadrado";
        btRun.className += " red-shadow"
        timer.disabled = samples.disabled = true;
    }else{
        btRun.style.backgroundColor = "#5cb85c";
        btCentro.className = "bt-triangulo";
        btRun.className = "bt-play round"
        timer.value = "00:00:00";
        timer.disabled = samples.disabled = false;    
    }
}

function twoDigits(n){
    return (n < 10) ? "0" + n : n;
}

//-----------------------------VALIDATORs--------------------------------

var REGEX_HOUR = /^([01]?[0-9]|2[0-3])(:[0-5]?[0-9]){2}$/;
var REGEX_SAMPLES = /([0-9](.{1}5{1})$)|[0-9]/;

function sampleValidator(samples){
    if(!REGEX_SAMPLES.test(samples.value)){
        samples.value = 1;
    }else{
        samples.value = ((samples.value % .5) != 0) ? 1 : (
            (samples.value > 1800) ? 1800 : samples.value
        )
    }
    restartKey();
}

function timerValidator(t){
    if(!REGEX_HOUR.test(t.value)){
        t.value = "00:00:00";
    }else{
        var a = t.value.match(/\d+/g);
        t.value = `${twoDigits(parseInt(a[0]))}:${twoDigits(parseInt(a[1]))}:${twoDigits(parseInt(a[2]))}`;
    }
    restartKey();
}

function filterKey(){
    document.onkeydown = (evObject) => {
        if(!/[0-9]|:|Backspace|ArrowLeft|ArrowRight|./.test(evObject.key)){
            return false;
        }
    };
}

function restartKey(){
    document.onkeydown = () => { return true; };
}

//---------------------------AUTHENTICATION------------------------------

const STOP = "stopExperiment";

function showAuth(){
    if(!play){
        document.getElementById("opaque").hidden = false;
        document.getElementById("float-credential").hidden = false;
        document.getElementById("tx-pass").focus();
    }else {
        connection.send(STOP);
    }
}

function auth(f){
    var send = setObjectToSend();
    if(send.pass.length > 0){
      connection.send(JSON.stringify(send));
    }else{
      alertResponse('Contraseña Vacía', 2);
    }
    f.reset();
    return false;
}

function setObjectToSend(){
    timer = document.getElementById("tx-timer").value;
    return {
        pass: document.getElementById("tx-pass").value,
        hh: parseInt(timer.slice(0,2)),
        mm: parseInt(timer.slice(3,5)),
        ss: parseInt(timer.slice(6,8)),
        samples: document.getElementById("tx-samples").value
    };
}

function cancel(){
    document.getElementById("opaque").hidden = true;
    document.getElementById("float-credential").hidden = true;
}

function alertResponse(response, stop){
    var txResponse = document.getElementById("tx-response");
    txResponse.innerHTML = response;
    txResponse.hidden = false;
    setTimeout(() => {
        txResponse.hidden = true;
    }, stop*1000);
}

//----------------------------DOWNLOAD--------------------------------

function disabledDownload(){
    var btDownload = document.getElementById("bt-download");
    btDownload.removeAttribute("href");
    btDownload.removeAttribute("download");
    btDownload.hidden = true;
}

function enabledDownload(p){
    var btDownload = document.getElementById("bt-download");
    btDownload.setAttribute("href", p);
    btDownload.setAttribute("download", "temp");
    btDownload.hidden = false;
}

//----------------------------GRAFICOS--------------------------------

var canvas = document.querySelectorAll("canvas");
for (var i = canvas.length - 1; i >= 0; i--) {
  canvas[i].width = canvas[i].parentElement.Width;
  canvas[i].height = canvas[i].parentElement.Height;
}

var maxDataPoints = 15

var grp_ef = document.getElementById("grp-ef");
var grp_sf = document.getElementById("grp-sf");
var grp_ec = document.getElementById("grp-ec");
var grp_sc = document.getElementById("grp-sc");


chart_ef = drawChart(grp_ef.getContext('2d'), 'Agua Fría In', 'rgba(54,162,235,1)');
chart_sf = drawChart(grp_sf.getContext('2d'), 'Agua Fría Out', 'rgba(255, 159, 64, 1)');
chart_ec = drawChart(grp_ec.getContext('2d'), 'Agua Caliente In', 'rgba(225,130,157,1)');
chart_sc = drawChart(grp_sc.getContext('2d'), 'Agua Caliente Out', 'rgba(111,205,205,1)');

function drawChart(chart, label, color){
    return new Chart(chart, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                data: [],
                label: label,
                pointBackgroundColor: color,
                borderColor: color.replace(/[^,]+(?=\))/, '0.2'),
                fill: false
            }]
        },
        options: {
            scales: {
                yAxes: [{
                    ticks: {
                        beginAtZero: true,
                        maxTicksLimit: 5,
                        stepSize: 10,
                        max: 100
                    }
                }]
            }
        }
    })
}

function plotGraphics(graphic, t){
  var today = new Date();
  var date = today.getHours() + ':' + today.getMinutes() + ':' + today.getSeconds()
  if (graphic.data.labels.length > maxDataPoints){
    removeData(graphic)
  }
  graphic.data.labels.push(date)
  graphic.data.datasets[0].data.push((t<100)?t:100)
  graphic.update()
}

function removeData(graph){
  graph.data.labels.shift()
  graph.data.datasets[0].data.shift()
}