(function(){
  // scroll-reveal: sections rise+fade in as they enter the viewport. Honors
  // reduced-motion (CSS already neutralizes .reveal there; we skip observing).
  var rm=window.matchMedia&&window.matchMedia('(prefers-reduced-motion:reduce)').matches;
  var rev=document.querySelectorAll('.reveal');
  if(rm||!('IntersectionObserver'in window)){rev.forEach(function(e){e.classList.add('in');});}
  else{var io=new IntersectionObserver(function(es){es.forEach(function(en){
    if(en.isIntersecting){en.target.classList.add('in');io.unobserve(en.target);}});},
    {threshold:.12,rootMargin:'0px 0px -8% 0px'});
    rev.forEach(function(e){io.observe(e);});}
  // carousels: prev/next scroll the swipe track by ~one viewport (touch/
  // trackpad swipe works natively via overflow scroll + snap)
  document.querySelectorAll('.carousel-wrap').forEach(function(w){
    var track=w.querySelector('.gallery.carousel');if(!track)return;
    var by=function(){return Math.max(track.clientWidth*0.8,240);};
    var p=w.querySelector('.prev'),n=w.querySelector('.next');
    if(p)p.addEventListener('click',function(){track.scrollBy({left:-by(),behavior:'smooth'});});
    if(n)n.addEventListener('click',function(){track.scrollBy({left:by(),behavior:'smooth'});});
  });
  /* click-to-play video. The page ships a poster and a button; the provider's
     iframe is created HERE, on the click, and never before — so a visitor who
     does not press play is not reported to YouTube or Vimeo for having read the
     page. That is the whole point of the block (see render/video.hpp); leaving
     the iframe in the markup would undo it silently.
     `data-embed` is written by the renderer from a parsed provider + id, so it
     is never author-supplied markup. */
  document.querySelectorAll('.videoblk').forEach(function(v){
    var btn=v.querySelector('.vplay');if(!btn)return;
    btn.addEventListener('click',function(){
      var src=v.getAttribute('data-embed');if(!src)return;
      var f=document.createElement('iframe');
      f.setAttribute('src',src);
      f.setAttribute('title',btn.getAttribute('aria-label')||'video');
      f.setAttribute('frameborder','0');
      f.setAttribute('allow','accelerometer;autoplay;encrypted-media;gyroscope;picture-in-picture');
      f.setAttribute('allowfullscreen','');
      f.setAttribute('referrerpolicy','strict-origin-when-cross-origin');
      v.classList.add('playing');
      btn.remove();
      v.appendChild(f);
    });
  });
  // lightbox
  var lb=document.getElementById('lightbox'),img=document.getElementById('lb-img'),
      cap=document.getElementById('lb-cap');
  document.querySelectorAll('.tile').forEach(function(a){
    a.addEventListener('click',function(e){e.preventDefault();
      img.src=a.getAttribute('href');cap.textContent=a.dataset.caption||'';lb.hidden=false;});
  });
  function close(){lb.hidden=true;img.src='';}
  lb.addEventListener('click',function(e){if(e.target===lb||e.target.className==='lb-close')close();});
  document.addEventListener('keydown',function(e){if(e.key==='Escape')close();});
  // live filter: one box per filterable card group
  document.querySelectorAll('.cards.filterable').forEach(function(group){
    var box=document.createElement('div');box.className='filter';
    var input=document.createElement('input');input.placeholder='Filter…';
    box.appendChild(input);group.parentNode.insertBefore(box,group);
    input.addEventListener('input',function(){
      var q=input.value.toLowerCase();
      group.querySelectorAll('.card').forEach(function(c){
        var hay=(c.dataset.tags+' '+c.textContent).toLowerCase();
        c.style.display=hay.indexOf(q)>-1?'':'none';});
    });
  });

  /* ── THE DOWNLOAD FOR YOUR COMPUTER ───────────────────────────────────────
     The author asked for the page to detect the visitor's operating system and
     offer the right build. `src/render/download.hpp` records why this is the
     only shape that request survives in, and the short version is the rule this
     block is built to keep: **it never hides one.**

     A `.wrow.platform-set` is a row the RENDERER decided is a set of per-platform
     downloads -- two or more blocks that named a computer -- and each candidate
     carries `data-platform`. We do exactly two things: move the matching one to
     the front of its row, and label it with the row's own `data-yours` text
     (which is in the page's language, because one app.js serves both).

     Not on a match, not on a miss, not on a browser that tells us nothing does
     anything get removed. With scripting off none of this runs at all and the
     author's order stands, which is the same guarantee the <noscript> rule buys
     everywhere else on the page. */
  var uad=navigator.userAgentData;
  var pstr=((uad&&uad.platform)||navigator.platform||'').toLowerCase();
  var uastr=(navigator.userAgent||'').toLowerCase();
  var fam='';
  /* A phone, a tablet and a Chromebook run none of these installers, and Android
     and Chrome OS both report themselves as Linux -- matching either would badge
     the Linux card "for your computer" on a device that cannot use it. A user
     agent is a guess; when the guess is unsafe the honest move is to say
     nothing and leave the page exactly as it was rendered. */
  if(!/android|iphone|ipad|ipod|cros/.test(pstr+' '+uastr)){
    if(pstr.indexOf('win')===0||pstr.indexOf('windows')>-1)fam='windows';
    else if(pstr.indexOf('mac')>-1)fam='macos';
    else if(pstr.indexOf('linux')>-1)fam='linux';
  }
  if(fam)document.querySelectorAll('.wrow.platform-set').forEach(function(row){
    var label=row.getAttribute('data-yours')||'';
    var cols=row.children,mine=null;
    for(var ci=0;ci<cols.length&&!mine;ci++){
      var a=cols[ci].querySelector('[data-platform]');
      if(!a)continue;
      /* The token is `<family>` or `<family>-<arch>`; only the family is
         matched. A browser will not tell you the architecture it is running on
         -- navigator.platform still says "Win32" on a 64-bit machine -- so the
         arch is carried for the author and the filename, never guessed at. */
      if(((a.getAttribute('data-platform')||'').split('-')[0])===fam)mine=cols[ci];
    }
    if(!mine)return;  // nothing here is for this visitor: leave the row alone
    if(label&&!mine.querySelector('.plat-yours')){
      var b=document.createElement('span');
      b.className='plat-yours';b.textContent=label;
      mine.insertBefore(b,mine.firstChild);
    }
    mine.className+=' is-yours';
    if(row.firstChild!==mine)row.insertBefore(mine,row.firstChild);
  });

  // ── LIVE DIRECTORIES ─────────────────────────────────────────────────────
  // A directory marked `live` refreshes from a small fragment under index/,
  // so updating one contact costs that file being republished rather than the
  // whole site being redeployed. See okf/concepts/platform/data-planes.md.
  //
  // PROGRESSIVE ENHANCEMENT, and every branch below preserves it: the cards
  // are ALREADY in the page, rendered at build time by the same C++ that wrote
  // the fragment. If there is no fetch, if it fails, if it 404s, or if it comes
  // back empty, we leave the page exactly as it was. This can make a directory
  // fresher; it must never make one worse.
  document.querySelectorAll('.cards[data-live]').forEach(function(group){
    var src=group.getAttribute('data-live');
    if(!src||!window.fetch) return;
    fetch(src,{cache:'no-cache'}).then(function(r){
      if(!r.ok) throw 0;
      return r.text();
    }).then(function(html){
      // NEVER blank a directory that has cards. A truncated or empty response
      // is a reason to keep what we have, not to show nothing.
      if(!html||!html.trim()) return;
      group.innerHTML=html;
      // the filter input is a SIBLING inserted above the group, so it survives
      // the swap — but whatever the visitor had typed no longer applies to the
      // new cards, so re-run it
      var box=group.previousElementSibling;
      var input=box&&box.className==='filter'?box.querySelector('input'):null;
      if(input&&input.value) input.dispatchEvent(new Event('input'));
    }).catch(function(){ /* keep the build-time cards */ });
  });

  // ── the marker outline: a port of src/gis/marker.hpp (keep in step). The
  // anchor (0,0) is the location: a pin or balloon stands on its tip. A pin is
  // a head of radius r, 2.2r above the tip, whose sides leave the head along
  // its tangents from the tip and curve in to a sharp point. ──────────────
  function mkOutline(f,r){
    var P=[],PI=Math.PI;
    function arc(cx,cy,rr,a0,a1,n){for(var i=0;i<=n;i++){var a=a0+(a1-a0)*i/n;P.push([cx+rr*Math.cos(a),cy+rr*Math.sin(a)]);}}
    function cub(p0,p1,p2,p3,n,skip){for(var i=skip?1:0;i<=n;i++){var t=i/n,u=1-t;
      P.push([u*u*u*p0[0]+3*u*u*t*p1[0]+3*u*t*t*p2[0]+t*t*t*p3[0],u*u*u*p0[1]+3*u*u*t*p1[1]+3*u*t*t*p2[1]+t*t*t*p3[1]]);}}
    var fx=0,fy=0;
    if(f==='pin'){var h=2.2*r,ph=Math.acos(r/h),C=[0,-h],T=[0,0];
      var Pl=[-r*Math.sin(ph),-h+r*Math.cos(ph)],Pr=[r*Math.sin(ph),-h+r*Math.cos(ph)];
      cub(T,[-0.18*r,-0.5*r],[Pl[0]+(T[0]-Pl[0])*0.45,Pl[1]+(T[1]-Pl[1])*0.45],Pl,6,false);P.pop();
      arc(C[0],C[1],r,PI/2+ph,PI/2-ph+2*PI,24);
      cub(Pr,[Pr[0]+(T[0]-Pr[0])*0.45,Pr[1]+(T[1]-Pr[1])*0.45],[0.18*r,-0.5*r],T,6,true);P.pop();
      fy=-h;}
    else if(f==='balloon'){var a=1.12*r,b=r,tl=0.62*r,tw=0.4*r,cr=0.42*r,top=-tl-2*b,bot=-tl;
      P.push([0,0]);P.push([-tw,bot]);
      arc(-a+cr,bot-cr,cr,PI/2,PI,6);arc(-a+cr,top+cr,cr,PI,1.5*PI,6);
      arc(a-cr,top+cr,cr,1.5*PI,2*PI,6);arc(a-cr,bot-cr,cr,0,PI/2,6);P.push([tw,bot]);fy=bot-b;}
    else if(f==='square'){var q=0.28*r;
      arc(-r+q,-r+q,q,PI,1.5*PI,4);arc(r-q,-r+q,q,1.5*PI,2*PI,4);arc(r-q,r-q,q,0,PI/2,4);arc(-r+q,r-q,q,PI/2,PI,4);}
    else if(f==='diamond'){var d=1.25*r;P=[[0,-d],[d,0],[0,d],[-d,0]];}
    else{arc(0,0,r,-PI/2,1.5*PI,24);P.pop();}
    return {p:P,fx:fx,fy:fy};
  }

  // ── the MAP widget: a hand-rolled READ-ONLY slippy map (canvas tiles +
  // markers). It can pan, zoom, and show names — it can never write. ──────
  document.querySelectorAll('.mapwidget').forEach(function(w){
    var mk=JSON.parse(w.dataset.markers||'[]');
    var shp=JSON.parse(w.dataset.shapes||'[]');
    var c0=(w.dataset.center||'44.05,-123.09').split(',');
    var lat=+c0[0],lon=+c0[1],z=Math.max(3,Math.min(18,+(w.dataset.zoom||13)));
    var tmpl=w.dataset.tiles||
      'https://basemaps.cartocdn.com/rastertiles/voyager/{z}/{x}/{y}.png';
    var cv=document.createElement('canvas');w.appendChild(cv);
    var pop=document.createElement('div');pop.className='mappop';w.appendChild(pop);
    var at=document.createElement('div');at.className='attrib';
    at.textContent=w.dataset.attrib||'© OpenStreetMap contributors © CARTO';
    w.appendChild(at);
    var ctx=cv.getContext('2d'),tiles={},drag=null;
    function mx(lo,zz){return (lo+180)/360*Math.pow(2,zz);}
    function my(la,zz){var r=la*Math.PI/180;
      return (1-Math.log(Math.tan(r)+1/Math.cos(r))/Math.PI)/2*Math.pow(2,zz);}
    function ilon(x,zz){return x/Math.pow(2,zz)*360-180;}
    function ilat(y,zz){var n=Math.PI-2*Math.PI*y/Math.pow(2,zz);
      return 180/Math.PI*Math.atan(0.5*(Math.exp(n)-Math.exp(-n)));}
    function tile(zz,tx,ty){var k=zz+'/'+tx+'/'+ty;
      if(tiles[k])return tiles[k].ok?tiles[k].img:null;
      var img=new Image();tiles[k]={img:img,ok:false};
      img.onload=function(){tiles[k].ok=true;draw();};
      img.src=tmpl.replace('{z}',zz).replace('{x}',tx).replace('{y}',ty);
      return null;}
    function draw(){
      var W=w.clientWidth,H=w.clientHeight;
      // 0-size guard: if the element isn't laid out yet (or is momentarily
      // hidden), retry next frame instead of sizing the canvas to 0 and
      // leaving a permanent blank/black box (the "black then works after
      // resize" bug, 2026-07-23).
      if(!W||!H){requestAnimationFrame(draw);return;}
      // only resize when it CHANGED — setting canvas.width clears the bitmap,
      // so redrawing at a stable size must not wipe it first
      if(cv.width!==W)cv.width=W;if(cv.height!==H)cv.height=H;
      ctx.fillStyle='#e8e6e1';ctx.fillRect(0,0,W,H);
      var cx=mx(lon,z),cy=my(lat,z),n=Math.pow(2,z);
      var x0=Math.floor(cx-W/512)-1,x1=Math.floor(cx+W/512)+1;
      var y0=Math.max(0,Math.floor(cy-H/512)-1),y1=Math.min(n-1,Math.floor(cy+H/512)+1);
      for(var ty=y0;ty<=y1;ty++)for(var tx=x0;tx<=x1;tx++){
        var im=tile(z,((tx%n)+n)%n,ty);
        if(im)ctx.drawImage(im,Math.round(W/2+(tx-cx)*256),Math.round(H/2+(ty-cy)*256));}
      // #3: drawn map SHAPES (under the markers) — rect/ellipse annotations
      shp.forEach(function(sp){
        var x1=W/2+(mx(sp.o1,z)-cx)*256,y1=H/2+(my(sp.a1,z)-cy)*256;
        var x2=W/2+(mx(sp.o2,z)-cx)*256,y2=H/2+(my(sp.a2,z)-cy)*256;
        var mnx=Math.min(x1,x2),mxx=Math.max(x1,x2);
        var mny=Math.min(y1,y2),mxy=Math.max(y1,y2);
        ctx.save();ctx.globalAlpha=0.2;ctx.fillStyle=sp.c;
        if(sp.e){ctx.beginPath();
          ctx.ellipse((mnx+mxx)/2,(mny+mxy)/2,(mxx-mnx)/2,(mxy-mny)/2,0,0,7);
          ctx.fill();ctx.globalAlpha=0.9;ctx.strokeStyle=sp.c;ctx.lineWidth=2;ctx.stroke();
        }else{ctx.fillRect(mnx,mny,mxx-mnx,mxy-mny);
          ctx.globalAlpha=0.9;ctx.strokeStyle=sp.c;ctx.lineWidth=2;
          ctx.strokeRect(mnx,mny,mxx-mnx,mxy-mny);}
        ctx.restore();});
      mk.forEach(function(m){
        var sx=W/2+(mx(m.lo,z)-cx)*256+(m.dx||0),sy=H/2+(my(m.la,z)-cy)*256+(m.dy||0);
        if(sx<-20||sx>W+20||sy<-20||sy>H+20)return;
        // marker SHAPE (matches the app): the outline is a port of
        // src/gis/marker.hpp's marker_outline — keep the two in step
        var R=7,f=m.s||'circle',o=mkOutline(f,(f==='pin'||f==='balloon')?8:R);
        if(f==='pin'||f==='balloon'){ctx.save();ctx.globalAlpha=0.24;ctx.fillStyle='#000';
          ctx.beginPath();ctx.ellipse(sx,sy,4.4,1.6,0,0,7);ctx.fill();ctx.restore();}
        ctx.beginPath();ctx.moveTo(sx+o.p[0][0],sy+o.p[0][1]);
        for(var i=1;i<o.p.length;i++)ctx.lineTo(sx+o.p[i][0],sy+o.p[i][1]);
        ctx.closePath();ctx.fillStyle=m.c;ctx.fill();
        ctx.lineWidth=1.6;ctx.strokeStyle='#fff';ctx.stroke();
        if(f==='pin'){ctx.beginPath();ctx.arc(sx+o.fx,sy+o.fy,2.9,0,7);ctx.fillStyle='#fff';ctx.fill();}
        m._sx=sx+o.fx;m._sy=sy+o.fy;});
    }
    function hit(e){var r=cv.getBoundingClientRect(),x=e.clientX-r.left,y=e.clientY-r.top;
      for(var i=0;i<mk.length;i++){var m=mk[i];
        if(m._sx!==undefined&&(x-m._sx)*(x-m._sx)+(y-m._sy)*(y-m._sy)<144)return m;}
      return null;}
    cv.addEventListener('mousedown',function(e){drag={x:e.clientX,y:e.clientY};});
    window.addEventListener('mouseup',function(){drag=null;});
    window.addEventListener('mousemove',function(e){
      if(drag){var cx=mx(lon,z),cy=my(lat,z);
        lon=ilon(cx-(e.clientX-drag.x)/256,z);lat=ilat(cy-(e.clientY-drag.y)/256,z);
        drag={x:e.clientX,y:e.clientY};draw();return;}
      var m=hit(e);if(m){var r=cv.getBoundingClientRect();
        pop.style.display='block';pop.style.left=(m._sx+12)+'px';pop.style.top=(m._sy-10)+'px';
        pop.textContent=m.n;}else pop.style.display='none';});
    cv.addEventListener('wheel',function(e){e.preventDefault();
      var nz=Math.max(3,Math.min(18,z+(e.deltaY<0?1:-1)));if(nz!==z){z=nz;draw();}},
      {passive:false});
    // draw when: laid out (rAF), fully loaded (fonts/CSS settled), and on
    // resize — the ResizeObserver debounced through rAF so a size change can
    // never feed back into a redraw loop
    var raf=0;function schedule(){if(raf)return;raf=requestAnimationFrame(function(){raf=0;draw();});}
    new ResizeObserver(schedule).observe(w);
    window.addEventListener('load',schedule);
    requestAnimationFrame(draw);
  });

  // ── the CALENDAR widget: month / week / 3-day, READ-ONLY. Entries link to
  // "add to Google Calendar"; the bar offers the .ics download. ───────────
  document.querySelectorAll('.calwidget').forEach(function(w){
    var ev=JSON.parse(w.dataset.events||'[]');
    var mode='month',anchor=new Date();anchor.setHours(0,0,0,0);
    var MN=['January','February','March','April','May','June','July','August',
            'September','October','November','December'];
    var DN=['Sun','Mon','Tue','Wed','Thu','Fri','Sat'];
    function ymd(d){return d.getFullYear()+'-'+String(d.getMonth()+1).padStart(2,'0')+
      '-'+String(d.getDate()).padStart(2,'0');}
    function on(d){var k=ymd(d);return ev.filter(function(e){return e.d===k;})
      .sort(function(a,b){return (a.s||'').localeCompare(b.s||'');});}
    /* The renderer computed this (see gcal_url in render/site.cpp). The version
       that lived here parsed the time itself and got it wrong for every
       12-hour value in the database — which is what a flier prints, so that was
       every event. One parser, host-side, where the .ics writer's already is. */
    function glink(e){return e.g||'';}
    function ent(e){var a=document.createElement('a');
      a.className='ent'+(e.i?' inc':'');a.style.background=e.c;
      a.textContent=(e.s?e.s+' ':'')+(e.i?'⚠ ':'')+e.n;
      a.href=glink(e);a.target='_blank';a.rel='noopener';
      a.title='Add to Google Calendar';return a;}
    function render(){
      w.innerHTML='';
      var bar=document.createElement('div');bar.className='cal-bar';
      [['month','Month'],['week','Week'],['3day','3 days']].forEach(function(mm){
        var b=document.createElement('button');b.textContent=mm[1];
        if(mode===mm[0])b.className='on';
        b.onclick=function(){mode=mm[0];render();};bar.appendChild(b);});
      var prev=document.createElement('button');prev.textContent='‹';
      var today=document.createElement('button');today.textContent='Today';
      var next=document.createElement('button');next.textContent='›';
      var step=mode==='month'?0:(mode==='week'?7:3);
      prev.onclick=function(){step?anchor.setDate(anchor.getDate()-step)
        :anchor.setMonth(anchor.getMonth()-1);render();};
      next.onclick=function(){step?anchor.setDate(anchor.getDate()+step)
        :anchor.setMonth(anchor.getMonth()+1);render();};
      today.onclick=function(){anchor=new Date();anchor.setHours(0,0,0,0);render();};
      bar.appendChild(prev);bar.appendChild(today);bar.appendChild(next);
      var ti=document.createElement('span');ti.className='cal-title';
      ti.textContent=MN[anchor.getMonth()]+' '+anchor.getFullYear();bar.appendChild(ti);
      var links=document.createElement('span');links.className='cal-links';
      /* the .ics filename comes from the WIDGET, not from this script: a
         bilingual site writes one calendar per language (the SUMMARY of a
         VEVENT is prose, and prose has a language), so a hardcoded name here
         would hand a Spanish reader the English file. */
      links.innerHTML='<a href="'+(w.dataset.ics||'calendar.ics')+'" download>⤓ .ics</a> '+
        '(import into Google/Apple/Outlook)';
      bar.appendChild(links);w.appendChild(bar);
      var now=new Date();now.setHours(0,0,0,0);
      /* A PHONE GETS AN AGENDA, not a crushed month grid (2026-08-20).
         Seven columns across a 340px screen is ~48px a cell: a date number and
         an ellipsis. The data is fine; the SHAPE is wrong for the device. So on
         a narrow screen the month view renders the same events as a dated list,
         which is what a person checking "what is on this week" wants anyway.
         The CSS hides the table at the same breakpoint, so a browser with JS
         off still gets something legible. */
      var narrow=window.matchMedia&&window.matchMedia('(max-width:640px)').matches;
      if(mode==='month'&&narrow){
        var ag=document.createElement('div');ag.className='agenda';
        ag.style.display='block';
        var m=anchor.getMonth(),y=anchor.getFullYear();
        var inMonth=ev.filter(function(e){var p=e.d.split('-');
          return +p[0]===y&&+p[1]===m+1;})
          .sort(function(a,b){return a.d.localeCompare(b.d)||
            (a.s||'').localeCompare(b.s||'');});
        if(!inMonth.length){var none=document.createElement('p');
          none.className='empty';none.textContent='Nothing this month.';
          ag.appendChild(none);}
        var byDay={};inMonth.forEach(function(e){(byDay[e.d]=byDay[e.d]||[]).push(e);});
        Object.keys(byDay).sort().forEach(function(k){
          var row=document.createElement('div');row.className='ag-day';
          var dd=new Date(k+'T12:00:00');
          var dt=document.createElement('div');dt.className='ag-date';
          dt.textContent=DN[dd.getDay()]+' '+dd.getDate();
          var it=document.createElement('div');it.className='ag-items';
          byDay[k].forEach(function(e){
            var a=document.createElement('a');
            a.href=glink(e);a.target='_blank';a.rel='noopener';
            var dot=document.createElement('span');dot.className='ag-dot';
            dot.style.background=e.c;a.appendChild(dot);
            a.appendChild(document.createTextNode(
              (e.s?e.s+' · ':'')+(e.i?'⚠ ':'')+e.n+(e.v?' · '+e.v:'')));
            it.appendChild(a);});
          row.appendChild(dt);row.appendChild(it);ag.appendChild(row);});
        w.appendChild(ag);
      }else if(mode==='month'){
        var t=document.createElement('table'),hr=t.insertRow();
        DN.forEach(function(d){var th=document.createElement('th');
          th.textContent=d;hr.appendChild(th);});
        var first=new Date(anchor.getFullYear(),anchor.getMonth(),1);
        var start=new Date(first);start.setDate(1-first.getDay());
        for(var r=0;r<6;r++){var row=t.insertRow();var any=false;
          for(var c=0;c<7;c++){var d=new Date(start);d.setDate(start.getDate()+r*7+c);
            var td=row.insertCell();
            if(d.getMonth()!==anchor.getMonth())td.className='other';
            else{any=true;
              if(d.getTime()===now.getTime())td.className='today';
              var dn=document.createElement('div');dn.className='dnum';
              dn.textContent=d.getDate();td.appendChild(dn);
              on(d).forEach(function(e){td.appendChild(ent(e));});}}
          if(!any&&r>3)t.deleteRow(-1);}
        w.appendChild(t);
      }else{
        var n=mode==='week'?7:3;
        var s=new Date(anchor);
        if(mode==='week')s.setDate(s.getDate()-s.getDay());
        var cols=document.createElement('div');cols.className='cols';
        cols.style.gridTemplateColumns='repeat('+n+',1fr)';
        for(var i=0;i<n;i++){var d=new Date(s);d.setDate(s.getDate()+i);
          var col=document.createElement('div');col.className='col';
          if(d.getTime()===now.getTime())col.className+=' today';
          var h4=document.createElement('h4');
          h4.textContent=DN[d.getDay()]+' '+d.getDate();col.appendChild(h4);
          var es=on(d);
          if(!es.length){var em=document.createElement('div');em.className='dnum';
            em.textContent='—';col.appendChild(em);}
          es.forEach(function(e){col.appendChild(ent(e));});
          cols.appendChild(col);}
        w.appendChild(cols);
      }
    }
    render();
    /* Rotating a phone, or dragging a desktop window narrow, crosses the
       breakpoint — and a widget that only picks its shape once shows the wrong
       one until a reload. Debounced, because resize fires continuously. */
    var rt;window.addEventListener('resize',function(){
      clearTimeout(rt);rt=setTimeout(render,150);});
  });
})();
