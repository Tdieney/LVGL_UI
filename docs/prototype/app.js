/* Browser-only design prototype. All telemetry, statuses and acknowledgements
 * are explicit fixtures, not a sensor algorithm or a hardware protocol. */
"use strict";
const paths = {
  home:'<path d="m3 10 9-7 9 7M5 9v12h5v-7h4v7h5V9"/>',
  chart:'<path d="M4 3v17h17M7 15l4-5 4 2 5-7"/>',
  devices:'<rect x="4" y="3" width="16" height="18" rx="3"/><path d="M8 7h8M8 11h8M8 15h2M15 15h1"/>',
  sliders:'<path d="M4 7h5m4 0h7M4 17h9m4 0h3"/><circle cx="11" cy="7" r="2"/><circle cx="15" cy="17" r="2"/>',
  radio:'<circle cx="12" cy="12" r="2"/><path d="M7 7a7 7 0 0 0 0 10M17 7a7 7 0 0 1 0 10M4 4a11 11 0 0 0 0 16M20 4a11 11 0 0 1 0 16"/>',
  timer:'<circle cx="12" cy="14" r="8"/><path d="M9 2h6m-3 0v4m0 4v5l3 2m4-12 2 2"/>',
  air:'<path d="M3 8h12a3 3 0 1 0-3-3M2 12h17a3 3 0 1 1-3 3M4 17h5a2 2 0 1 1-2 2"/>',
  thermometer:'<path d="M9 14V5a3 3 0 0 1 6 0v9a5 5 0 1 1-6 0Z"/><path d="M12 8v9"/><circle cx="12" cy="18" r="1"/>',
  drop:'<path d="M12 3S5 11 5 15a7 7 0 0 0 14 0c0-4-7-12-7-12Z"/><path d="M8 15a4 4 0 0 0 4 4"/>',
  purifier:'<rect x="5" y="2" width="14" height="20" rx="4"/><path d="M8 7h8M8 10h8M8 14h1m3 0h1m3 0h.1M8 17h1m3 0h1m3 0h.1"/>',
  fan:'<circle cx="12" cy="12" r="2"/><path d="M11 10C4 3 14-2 16 5c1 2-1 4-3 5M14 12c10-2 10 9 3 8-3 0-4-3-4-6M11 14c-3 9-12 3-7-2 2-2 4-1 6-1"/>',
  light:'<path d="m9 3 8 4-5 9-8-4 5-9ZM15 12l4 8M12 21h9M7 15l-2 3M3 14l-2 1"/>',
  plug:'<path d="M8 2v5m8-5v5M6 7h12v3a6 6 0 0 1-12 0V7Zm6 9v6"/>',
  heat:'<path d="M6 20V9m6 11V9m6 11V9M4 20h16M8 5V2m8 3V2"/>',
  check:'<path d="m7 12 3 3 7-7"/><circle cx="12" cy="12" r="9"/>',
  warning:'<path d="m12 3 10 18H2L12 3Zm0 6v5m0 3v.1"/>',
  close:'<path d="m6 6 12 12M18 6 6 18"/>',
  arrow:'<path d="m9 5 7 7-7 7"/>',
  chip:'<rect x="6" y="6" width="12" height="12" rx="2"/><path d="M9 2v4m6-4v4M9 18v4m6-4v4M2 9h4m-4 6h4m12-6h4m-4 6h4"/>'
};
const icon = name => `<svg class="icon" viewBox="0 0 24 24" aria-hidden="true">${paths[name] || paths.plug}</svg>`;
const metrics = {
  co2:{label:'CO₂',unit:'ppm',icon:'air',digits:0},
  voc:{label:'VOC',unit:'',icon:'air',digits:0},
  temperature:{label:'Temperature',unit:'°C',icon:'thermometer',digits:1},
  humidity:{label:'Humidity',unit:'%',icon:'drop',digits:0}
};
const presets = [
  ['Air Purifier','purifier'],['Ventilation Fan','fan'],['Humidifier','drop'],
  ['Dehumidifier','drop'],['Heater','heat'],['Light','light'],
  ['Smart Socket','plug'],['Generic Device','devices']
];
// Values and category labels are intentionally independent sample inputs.
const fixtures = {
  moderate:{co2:780,voc:120,temperature:24.6,humidity:48,raw:320,co2Status:'Good',vocStatus:'Moderate'},
  good:{co2:620,voc:82,temperature:23.8,humidity:46,raw:180,co2Status:'Good',vocStatus:'Good'},
  poor:{co2:1450,voc:260,temperature:25.1,humidity:49,raw:670,co2Status:'Poor',vocStatus:'Poor'}
};
const state = {
  page:'home',metric:'co2',scenario:'moderate',empty:false,linkBars:4,
  devices:[
    {name:'Air Purifier',preset:0,on:true,mode:'Auto'},
    {name:'Ventilation Fan',preset:1,on:false,mode:'Auto'},
    {name:'Humidifier',preset:2,on:false,mode:'Auto'},
    {name:'Desk Light',preset:5,on:true,mode:'Manual'}
  ],history:[],generation:0
};
const $ = selector => document.querySelector(selector);
const value = () => fixtures[state.scenario] || fixtures.moderate;
const offline = () => state.scenario === 'offline';
const missing = key => offline() || (state.scenario === 'sensor' && ['co2','voc','raw'].includes(key));
const fmt = (key, number) => number.toFixed(metrics[key].digits);
function seedHistory(){
  state.history = Array.from({length:16},(_,i)=>{
    const v=value(), factor=[-.12,-.09,-.1,-.06,-.03,.01,.04,.06,.13,.11,.08,.06,.04,.02,.01,0][i];
    return {minute:14*60+17+i,co2:Math.round(v.co2*(1+factor)),voc:Math.round(v.voc*(1+factor*2)),temperature:+(v.temperature+factor*3).toFixed(1),humidity:Math.round(v.humidity+factor*8)};
  });
  if(offline())state.history.slice(-3).forEach(row=>Object.keys(metrics).forEach(key=>row[key]=null));
  if(state.scenario==='sensor')state.history.slice(-3).forEach(row=>{row.co2=null;row.voc=null;});
}
function status(label){return `<span class="status ${label.toLowerCase()}">${label}</span>`;}
function deviceState(d){return offline()||d.error?'Unknown':d.pending?'Sending…':'';}
function toggle(d,i){
  if(offline())return `<button class="switch" disabled aria-label="${d.name} output unavailable">—</button>`;
  if(d.error)return `<button class="retry" data-toggle="${i}" aria-label="Retry ${d.name} command">Retry</button>`;
  return `<button class="switch ${d.pending?'pending':''}" role="switch" aria-label="${d.name} power" aria-checked="${!!d.on}" data-toggle="${i}" ${d.pending?'disabled':''}><span class="switch-track"></span></button>`;
}
function currentMetric(key){return missing(key)?'—':fmt(key,value()[key]);}
function home(){
  return `<div class="home-layout">
    ${['co2','voc'].map(key=>{
      const quality=missing(key)?'Unknown':value()[key+'Status'];
      const note=quality==='Unknown'?(offline()?'Connection lost':'Sensor unavailable'):quality==='Good'?(key==='co2'?'Fresh air':'Low VOC level'):'Consider ventilating';
      return `<button class="card sensor-card sensor-${key}" data-metric="${key}" aria-label="View ${metrics[key].label} trend"><span class="sensor-heading"><span class="sensor-label">${metrics[key].label}</span>${icon(metrics[key].icon)}</span><span class="sensor-reading"><span class="metric-value">${currentMetric(key)}</span>${metrics[key].unit?`<span class="unit">${metrics[key].unit}</span>`:''}</span><span class="sensor-footer">${status(quality)}<span class="sensor-note">${note}</span></span></button>`;
    }).join('')}
    <section class="comfort-stack" aria-label="Room comfort">${['temperature','humidity'].map(key=>`<button class="card comfort-card comfort-${key}" data-metric="${key}" aria-label="View ${metrics[key].label} trend"><span class="icon-box">${icon(metrics[key].icon)}</span><span class="comfort-info"><span class="metric-label">${metrics[key].label}</span><span class="metric-value">${currentMetric(key)}<span class="unit">${metrics[key].unit}</span></span><span class="comfort-caption" style="display:block">${offline()?'Data unavailable':'Comfortable'}</span></span></button>`).join('')}</section>
    <section class="device-section" aria-label="Device controls"><div class="quick-devices">${state.devices.map((d,i)=>`<article class="device-tile ${d.on&&!offline()&&!d.error?'on':''}"><div class="device-tile-head">${icon(presets[d.preset][1])}<span class="device-name">${d.name}</span></div><div class="device-tile-bottom"><div>${deviceState(d)?`<div class="device-state">${deviceState(d)}</div>`:''}<div class="mode-label">${d.mode}</div></div>${toggle(d,i)}</div></article>`).join('')}</div></section>
  </div>`;
}
function timeLabel(minute){return `${Math.floor(minute/60).toString().padStart(2,'0')}:${(minute%60).toString().padStart(2,'0')}`;}
function chart(){
  const key=state.metric, m=metrics[key], rows=state.history;
  const valid=rows.filter(row=>row[key]!==null);
  if(!valid.length)return '<div class="empty-chart">'+icon('chart')+'<strong>No readings yet</strong><span>This session is empty. Choose a scenario to load a sample session.</span></div>';
  const numbers=valid.map(row=>row[key]), min=Math.min(...numbers), max=Math.max(...numbers);
  const span=Math.max(max-min,key==='temperature'?1:10), bottom=min-span*.2, top=max+span*.2;
  const x=i=>55+i*635/15, y=n=>109-(n-bottom)/(top-bottom)*96;
  let drawing='',connected=false;
  rows.forEach((row,i)=>{if(row[key]===null){connected=false;return;}drawing+=`${connected?'L':'M'}${x(i).toFixed(1)},${y(row[key]).toFixed(1)} `;connected=true;});
  return `<svg viewBox="0 0 720 148" role="group" aria-label="${m.label} history. Last 15 minutes. Use Tab to inspect samples.">
    ${[0,.5,1].map(f=>{const n=bottom+(top-bottom)*f;return `<line x1="55" y1="${y(n)}" x2="692" y2="${y(n)}" stroke="var(--line)" stroke-dasharray="3 4"/><text x="0" y="${y(n)+4}">${fmt(key,n)}</text>`;}).join('')}
    <path d="${drawing}" stroke="var(--blue)" fill="none" stroke-width="2.5" stroke-linejoin="round" stroke-linecap="round"/>
    ${[0,5,10,15].map(i=>`<text x="${x(i)}" y="139" text-anchor="${i===0?'start':i===15?'end':'middle'}">${timeLabel(14*60+17+i)}</text>`).join('')}
    ${rows.map((row,i)=>row[key]===null?'':`<circle cx="${x(i)}" cy="${y(row[key])}" r="3" fill="var(--blue)"/><circle class="chart-hit" cx="${x(i)}" cy="${y(row[key])}" r="12" tabindex="0" role="button" aria-label="${timeLabel(row.minute)}, ${fmt(key,row[key])} ${m.unit}" data-sample="${i}"/>`).join('')}
    </svg><div id="chart-tooltip" class="chart-tooltip" role="status" hidden></div>`;
}
function trends(){
  const key=state.metric,m=metrics[key],numbers=state.history.map(row=>row[key]).filter(n=>n!==null);
  return `<div class="page-title"><h1>Trends</h1></div>
    <div class="metric-tabs" aria-label="Select trend metric">${Object.entries(metrics).map(([key,m])=>`<button data-metric="${key}" aria-pressed="${key===state.metric}">${m.label}</button>`).join('')}</div>
    <section class="card chart-card"><div class="chart-top"><div class="chart-summary">${[['Current',state.empty?'—':currentMetric(key)],['Min',numbers.length?fmt(key,Math.min(...numbers)):'—'],['Max',numbers.length?fmt(key,Math.max(...numbers)):'—']].map(([label,n])=>`<div class="stat"><small>${label}</small><strong>${n}<span class="unit">${m.unit}</span></strong></div>`).join('')}</div>${missing(key)?'<span class="chart-unavailable">Data unavailable</span>':''}</div><div class="plot">${chart()}</div></section>`;
}
function devices(){return `<div class="page-title"><h1>Devices</h1></div>
  <div class="device-grid">${state.devices.map((d,i)=>`<article class="card device-large"><div class="device-large-top"><span class="icon-box">${icon(presets[d.preset][1])}</span><div class="device-name">${d.name}</div><button class="configure" data-configure="${i}" aria-label="Configure ${d.name}">${icon('sliders')}</button></div><div class="device-large-bottom"><div><span class="mode-label">${d.mode}</span>${deviceState(d)?`<span class="device-state">${deviceState(d)}</span>`:''}</div>${toggle(d,i)}</div></article>`).join('')}</div>`;}
function signalBars(bars){
  return `<span class="signal-bars" aria-hidden="true"><span class="bar bar-1${bars>=1?' active':''}"></span><span class="bar bar-2${bars>=2?' active':''}"></span><span class="bar bar-3${bars>=3?' active':''}"></span><span class="bar bar-4${bars>=4?' active':''}"></span></span>`;
}
function render(){
  const link=$('#link-status');
  const isOffline = offline() || state.linkBars === 0;
  const bars = isOffline ? 0 : (state.linkBars !== undefined ? state.linkBars : 4);
  link.innerHTML = signalBars(bars) + (isOffline ? 'Disconnected' : 'Connected');
  link.classList.toggle('offline', isOffline);
  $('#content').innerHTML=({home,trends,devices})[state.page]();
  document.querySelectorAll('.navigation button').forEach(b=>{if(b.dataset.page===state.page)b.setAttribute('aria-current','page');else b.removeAttribute('aria-current');});
}
function announce(message){const toast=$('#toast');toast.textContent=message;toast.classList.add('visible');clearTimeout(announce.timer);announce.timer=setTimeout(()=>toast.classList.remove('visible'),2600);}
function sendCommand(index){
  const d=state.devices[index];if(offline()||d.pending)return;
  const generation=state.generation,target=!d.on,automatic=d.mode==='Auto';
  d.mode='Manual';d.pending=true;d.error=false;render();
  announce(automatic?'Manual control selected. Sending to node…':'Sending to node…');
  setTimeout(()=>{
    if(generation!==state.generation)return;
    d.pending=false;
    if(state.scenario==='timeout'){d.error=true;announce('No response from node. Output state unknown.');}
    else{d.on=target;announce(`${d.name} ${d.on?'on':'off'} · node confirmed`);}
    render();
    document.querySelector(`[data-toggle="${index}"]`)?.focus({preventScroll:true});
  },700);
}
let editing=-1,editMode='Manual';
function configure(index){
  editing=index;const d=state.devices[index];editMode=d.mode;
  const dialog=$('#device-dialog');
  dialog.innerHTML=`<div class="panel-heading"><h2 id="dialog-title">Device settings</h2><button class="close" id="close-device" aria-label="Close device settings">${icon('close')}</button></div><label for="device-type">Device type</label><select id="device-type">${presets.map(([name],i)=>`<option value="${i}" ${i===d.preset?'selected':''}>${name}</option>`).join('')}</select><label>Control mode</label><div class="mode-switch">${['Manual','Auto'].map(mode=>`<button data-mode="${mode}" aria-pressed="${mode===editMode}">${mode}</button>`).join('')}</div><p class="dialog-note">${offline()?'Connection lost. Settings unavailable.':'Manual control stays active until you select Auto.'}</p><button class="primary" id="save-device" ${offline()?'disabled':''}>Save settings</button>`;
  dialog.showModal();positionDialog();
}
function showSample(index){
  const row=state.history[index],key=state.metric,tooltip=$('#chart-tooltip');
  if(!tooltip||!row||row[key]===null)return;
  tooltip.hidden=false;tooltip.textContent=`${timeLabel(row.minute)} · ${fmt(key,row[key])} ${metrics[key].unit}`;
}
document.addEventListener('click',event=>{
  const b=event.target.closest('button,[data-sample]');if(!b)return;
  if(b.dataset.page){state.page=b.dataset.page;render();$('#content').focus({preventScroll:true});}
  else if(b.dataset.metric){state.metric=b.dataset.metric;state.page='trends';render();document.querySelector(`.metric-tabs [data-metric="${state.metric}"]`).focus({preventScroll:true});}
  else if(b.dataset.toggle!==undefined)sendCommand(+b.dataset.toggle);
  else if(b.dataset.configure!==undefined)configure(+b.dataset.configure);
  else if(b.id==='close-device')$('#device-dialog').close();
  else if(b.dataset.mode){editMode=b.dataset.mode;document.querySelectorAll('[data-mode]').forEach(el=>el.setAttribute('aria-pressed',String(el.dataset.mode===editMode)));}
  else if(b.id==='save-device'){
    if(offline())return;const d=state.devices[editing],preset=+$('#device-type').value;
    if(preset!==d.preset){d.preset=preset;d.name=presets[preset][0];}
    d.mode=editMode;$('#device-dialog').close();render();announce('Device settings updated for this preview.');
    document.querySelector(`[data-configure="${editing}"]`)?.focus();
  }
  else if(b.dataset.sample!==undefined)showSample(+b.dataset.sample);
});
document.addEventListener('focusin',event=>{if(event.target.dataset.sample!==undefined)showSample(+event.target.dataset.sample);});
document.addEventListener('keydown',event=>{
  if(['Enter',' '].includes(event.key)&&event.target.dataset.sample!==undefined){event.preventDefault();showSample(+event.target.dataset.sample);}
});
$('#scenario')?.addEventListener('change',event=>{
  state.scenario=event.target.value;state.empty=false;state.generation++;
  state.devices.forEach(d=>{d.pending=false;d.error=false;});
  if($('#device-dialog').open)$('#device-dialog').close();
  seedHistory();render();announce(state.scenario==='timeout'?'Try a device switch to preview a command timeout.':'Sample scenario loaded.');
});
$('#link-bars')?.addEventListener('change',event=>{
  state.linkBars=+event.target.value;
  render();
  announce(`Signal level set to ${state.linkBars===0?'Disconnected':state.linkBars+' bar'+(state.linkBars>1?'s':'')}.`);
});
$('#reset-session')?.addEventListener('click',()=>{state.history=[];state.empty=true;render();announce('Session history cleared. Choose a scenario to load sample data.');});
$('#replay-splash')?.addEventListener('click',()=>{
  const splash=$('#splash');clearTimeout(splash.timer);splash.hidden=true;void splash.offsetWidth;splash.hidden=false;
  splash.timer=setTimeout(()=>{splash.hidden=true;},2500);
});
function positionDialog(){
  const dialog=$('#device-dialog');if(!dialog.open)return;
  if(document.body.classList.contains('capture-mode')){
    dialog.style.left='400px';dialog.style.top='240px';dialog.style.transform='translate(-50%,-50%)';return;
  }
  const rect=$('#display').getBoundingClientRect();
  dialog.style.left=`${rect.left+rect.width/2}px`;dialog.style.top=`${rect.top+rect.height/2}px`;
  dialog.style.transform=`translate(-50%,-50%) scale(${rect.width/800})`;
}
function resize(){
  if(document.body.classList.contains('capture-mode')){
    $('#display').style.transform='none';$('#viewport').style.width='800px';$('#viewport').style.height='480px';positionDialog();return;
  }
  const scale=Math.min(1,(window.innerWidth-20)/800);$('#display').style.transform=`scale(${scale})`;$('#viewport').style.width=`${800*scale}px`;$('#viewport').style.height=`${480*scale}px`;positionDialog();
}
window.addEventListener('resize',resize);
// performance.now() is monotonic and elapsed time is never reset by navigation,
// scenario selection, clearing chart history or replaying the splash.
function formatUptime(seconds){
  const hours=Math.floor(seconds/3600),minutes=Math.floor(seconds/60)%60;
  return [hours,minutes,Math.floor(seconds)%60].map(n=>String(n).padStart(2,'0')).join(':');
}
function updateUptime(){ $('#uptime').textContent=formatUptime(performance.now()/1000); }
updateUptime();
setInterval(updateUptime,1000);
document.addEventListener('visibilitychange',updateUptime);
document.querySelectorAll('[data-icon]').forEach(el=>el.innerHTML=icon(el.dataset.icon));
const urlParams = new URLSearchParams(window.location.search);
if (urlParams.has('capture')) document.body.classList.add('capture-mode');
if (urlParams.get('page')) state.page = urlParams.get('page');
if (urlParams.get('scenario')) state.scenario = urlParams.get('scenario');
if (urlParams.has('bars')) state.linkBars = parseInt(urlParams.get('bars'), 10);
seedHistory();render();resize();
if (urlParams.get('dialog') === '1') {
  configure(1);
}
