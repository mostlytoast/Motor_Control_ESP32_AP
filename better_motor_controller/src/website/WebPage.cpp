
#include "WebPage.h"

// ============================================================
// COMPLETE WEB PAGE
// ============================================================

const char index_html[] PROGMEM = R"WEBPAGE(


<!DOCTYPE HTML>

<html>

<head>

<meta name="viewport"
      content="width=device-width, initial-scale=1">

<title>ESP32 Motor Control</title>

<style>

html {
  font-family: Arial;
  text-align: center;
}

body {
  max-width: 650px;
  margin: 0 auto;
  padding: 10px 15px 40px;
}

h2 {
  font-size: 2rem;
}

p {
  font-size: 1.2rem;
  margin: 20px 0;
}

.slider {
  width: 95%;
  height: 40px;
  background: #FFD65C;
  border: 3px solid #003249;
  border-radius: 20px;
  outline: none;
}

.value {
  font-weight: bold;
  color: #003249;
}

#direction {
  font-weight: bold;
  color: #003249;
}

.button {
  width: 80%;
  padding: 18px;
  font-size: 1.2rem;
  font-weight: bold;
  border: 3px solid #003249;
  border-radius: 15px;
  cursor: pointer;
  color: white;
}

.off {
  background: #777;
}

.on {
  background: #00a83b;
  box-shadow: 0 0 15px #00ff55;
}

.stop {
  background: #b00020;
}

.setup {
  margin-top: 35px;
  padding: 15px;
  border: 3px solid #003249;
  border-radius: 15px;
  background: #f3f3f3;
}

.controller {
  background: white;
  border: 2px solid #aaa;
  border-radius: 10px;
  margin: 8px 0;
  padding: 12px;
  text-align: left;
}

.active {
  border: 3px solid #00a83b;
  background: #eafff0;
}

.smallButton {
  padding: 9px 14px;
  margin: 4px;
  border: 0;
  border-radius: 8px;
  color: white;
  background: #003249;
  cursor: pointer;
}

.danger {
  background: #b00020;
}

#pairStatus {
  font-weight: bold;
  color: #003249;
}

.serialSection {
  margin-top: 35px;
  text-align: left;
}

#serialTerminal {
  width: 100%;
  height: 300px;
  box-sizing: border-box;
  background: #111;
  color: #00ff66;
  border: 3px solid #003249;
  border-radius: 12px;
  padding: 15px;
  font-family: monospace;
  overflow-y: auto;
}

.trackMapSection {
  margin-top: 35px;
  padding: 15px;
  border: 3px solid #003249;
  border-radius: 15px;
  background: #f3f3f3;
}

#trackMapContainer {
  width: 100%;
  height: 600px;
  overflow: hidden;
  position: relative;
  background: #d9d9d9;
  border: 3px solid #003249;
  border-radius: 12px;
}

#trackMapSvg {
  width: 100%;
  height: 100%;

  background:
    linear-gradient(
      45deg,
      #e4e4e4 25%,
      transparent 25%
    ),
    linear-gradient(
      -45deg,
      #e4e4e4 25%,
      transparent 25%
    ),
    linear-gradient(
      45deg,
      transparent 75%,
      #e4e4e4 75%
    ),
    linear-gradient(
      -45deg,
      transparent 75%,
      #e4e4e4 75%
    );

  background-size: 40px 40px;
  background-position:
    0 0,
    0 20px,
    20px -20px,
    -20px 0;
}

.mapTrack {
  stroke: #444;
  stroke-width: 14;
  stroke-linecap: round;
  fill: none;
}

.mapTrackCenter {
  stroke: #c9c9c9;
  stroke-width: 4;
  stroke-linecap: round;
  fill: none;
}

.mapTag {
  fill: #00a83b;
  stroke: white;
  stroke-width: 3;
}

.mapTag.unconnected {
  fill: #b00020;
}

.mapTrackLabel {
  font-family: Arial;
  font-size: 14px;
  font-weight: bold;
  fill: #003249;
}

.mapTagLabel {
  font-family: monospace;
  font-size: 10px;
  fill: #111;
}

.mapConnection {
  stroke: #777;
  stroke-width: 3;
  stroke-dasharray: 8 6;
}

.mapControls {
  margin: 10px;
}

.mapButton {
  padding: 10px 15px;
  margin: 4px;
  border: 0;
  border-radius: 8px;
  background: #003249;
  color: white;
  cursor: pointer;
}

</style>

</head>

<body>


<!-- =========================================================
     MOTOR CONTROL
     ========================================================= -->

<h2>ESP32 Motor Control</h2>

<p>
Motor Speed:
<span id="speedValue">0</span>
</p>

<input
  type="range"
  id="speedSlider"
  min="-255"
  max="255"
  value="0"
  step="1"
  class="slider"
  oninput="updateSpeed(this.value)"
>

<p>
Direction:
<span id="direction">STOPPED</span>
</p>

<p>
Ramp Time:
<span id="rampValue">5.0</span>
seconds
</p>

<input
  type="range"
  id="rampSlider"
  min="0"
  max="10"
  value="5"
  step="0.1"
  class="slider"
  oninput="updateRamp(this.value)"
>


<!-- =========================================================
     LED CONTROL
     ========================================================= -->

<h2>LED Lights</h2>

<p>

<button
  id="led1Button"
  class="button off"
  onclick="toggleLED(1)"
>
LED 1 OFF
</button>

</p>

<p>

<button
  id="led2Button"
  class="button off"
  onclick="toggleLED(2)"
>
LED 2 OFF
</button>

</p>

<p>

<button
  id="audioButton"
  class="button off"
  onclick="toggleAudio()"
>
AUDIO OFF
</button>

</p>

<p>

<button
  id="autoButton"
  class="button off"
  onclick="toggleAuto()"
>
AUTO OFF
</button>

</p>


<!-- =========================================================
     TRACK MAP
     ========================================================= -->

<div class="trackMapSection">

<h2>Track Map</h2>

<div class="mapControls">

<button
  class="mapButton"
  onclick="refreshTrackMap()"
>
REFRESH MAP
</button>

<button
  class="mapButton"
  onclick="resetMapView()"
>
RESET VIEW
</button>

<span>
Tracks:
<b id="mapTrackCount">0</b>
</span>

</div>

<div id="trackMapContainer">

<svg
  id="trackMapSvg"
  viewBox="0 0 1000 600"
  preserveAspectRatio="xMidYMid meet"
>
</svg>

</div>

</div>


<!-- =========================================================
     CONTROLLER SETUP
     ========================================================= -->

<div class="setup">

<h2>Controller Setup</h2>

<p>
Receiver:
<b>
%RECEIVER_NAME%
</b>
</p>

<p>
Receiver MAC:
<b>
%RECEIVER_MAC%
</b>
</p>

<p>
Active Controller:
<b>
<span id="activeController">
Loading...
</span>
</b>
</p>

<p>
Pairing:
<span id="pairStatus">
Loading...
</span>
</p>

<button
  class="smallButton"
  onclick="togglePairing()"
>
OPEN/CLOSE PAIRING
</button>

<button
  class="smallButton"
  onclick="refreshControllers()"
>
REFRESH
</button>

<div id="controllers">
Loading controllers...
</div>

</div>


<!-- =========================================================
     SERIAL TERMINAL
     ========================================================= -->

<div class="serialSection">

<h2>Serial Terminal</h2>

<div id="serialTerminal">
Connecting...
</div>

<button
  class="smallButton danger"
  onclick="clearSerial()"
>
CLEAR TERMINAL
</button>

</div>


<script>

// ============================================================
// GLOBAL TRACK MAP VARIABLES
// ============================================================

let trackMapData = null;

const MAP_WIDTH = 1000;
const MAP_HEIGHT = 600;

const MAP_PADDING = 80;

// World -> SVG transform.
// These are calculated from the backend positions.
let mapTransform = {
  minX: 0,
  minY: 0,
  scale: 1
};


// ============================================================
// SVG HELPER
// ============================================================

function svgElement(name, attributes) {

  const element =
    document.createElementNS(
      "http://www.w3.org/2000/svg",
      name
    );

  if (attributes) {

    Object.keys(attributes).forEach(
      function(key) {

        element.setAttribute(
          key,
          attributes[key]
        );

      }
    );

  }

  return element;
}


// ============================================================
// DRAW LINE
// ============================================================

function drawLine(
  parent,
  x1,
  y1,
  x2,
  y2,
  className
) {

  const line =
    svgElement(
      "line",
      {
        x1: x1,
        y1: y1,
        x2: x2,
        y2: y2,
        class: className
      }
    );

  parent.appendChild(line);

  return line;
}


// ============================================================
// DRAW PATH
// ============================================================

function drawPath(
  parent,
  d,
  className
) {

  const path =
    svgElement(
      "path",
      {
        d: d,
        class: className
      }
    );

  parent.appendChild(path);

  return path;
}


// ============================================================
// REFRESH TRACK MAP
// ============================================================

function refreshTrackMap() {

  const xhr =
    new XMLHttpRequest();

  xhr.open(
    "GET",
    "/tracks",
    true
  );

  xhr.onload = function() {

    if (xhr.status !== 200) {

      console.error(
        "Failed to load /tracks:",
        xhr.status
      );

      return;
    }

    try {

      trackMapData =
        JSON.parse(
          xhr.responseText
        );

    }
    catch (error) {

      console.error(
        "Invalid /tracks JSON:",
        error
      );

      return;
    }
    console.log(trackMapData);
    if (
      !trackMapData ||
      !Array.isArray(
        trackMapData.tracks
      )
    ) {

      console.error(
        "Invalid track map data"
      );

      return;
    }

    document.getElementById(
      "mapTrackCount"
    ).textContent =
      trackMapData.tracks.length;

    calculateMapTransform();

    drawTrackMap();

  };

  xhr.onerror = function() {

    console.error(
      "Unable to request /tracks"
    );

  };

  xhr.send();

}


// ============================================================
// GET BACKEND TRACK POSE
// ============================================================
function getTrackPose(track) {

  if (!track || !track.pose) {
    return null;
  }

  const entranceAX = Number(track.pose.entranceAX);
  const entranceAY = Number(track.pose.entranceAY);
  const exitAX = Number(track.pose.exitAX);
  const exitAY = Number(track.pose.exitAY);
  const entranceAHeading = Number(track.pose.entranceAHeading);
  const exitAHeading = Number(track.pose.exitAHeading);

  const entranceBX = Number(track.pose.entranceBX);
  const entranceBY = Number(track.pose.entranceBY);
  const exitBX = Number(track.pose.exitBX);
  const exitBY = Number(track.pose.exitBY);
  const entranceBHeading = Number(track.pose.entranceBHeading);
  const exitBHeading = Number(track.pose.exitBHeading);

  if (
    !Number.isFinite(entranceAX) ||
    !Number.isFinite(entranceAY) ||
    !Number.isFinite(exitAX) ||
    !Number.isFinite(exitAY) ||
    !Number.isFinite(entranceBX) ||
    !Number.isFinite(entranceBY) ||
    !Number.isFinite(exitBX) ||
    !Number.isFinite(exitBY)
  ) {
    return null;
  }

  // Draw as soon as we have real coordinates, regardless of
  // whether the backend also sets a separate "positioned" flag.
  return {
    entranceAX, entranceAY,
    exitAX, exitAY,
    entranceAHeading: Number.isFinite(entranceAHeading) ? entranceAHeading : 0,
    exitAHeading: Number.isFinite(exitAHeading) ? exitAHeading : 0,
    entranceBX, entranceBY,
    exitBX, exitBY,
    entranceBHeading: Number.isFinite(entranceBHeading) ? entranceBHeading : 0,
    exitBHeading: Number.isFinite(exitBHeading) ? exitBHeading : 0
  };
}

// ============================================================
// WORLD -> SVG
//
// The backend owns the real coordinate system.
// This function only converts those coordinates into
// the 1000 x 600 SVG viewport.
// ============================================================

function worldToMap(
  x,
  y
) {

  return {

    x:
      MAP_PADDING +
      (x - mapTransform.minX) *
      mapTransform.scale,

    y:
      MAP_PADDING +
      (y - mapTransform.minY) *
      mapTransform.scale

  };

}


// ============================================================
// CALCULATE WORLD -> SVG TRANSFORM
// ============================================================

function calculateMapTransform() {

  mapTransform = {

    minX: 0,
    minY: 0,
    scale: 1

  };

  if (
    !trackMapData ||
    !Array.isArray(
      trackMapData.tracks
    )
  ) {

    return;

  }

  let minX = Infinity;
  let maxX = -Infinity;

  let minY = Infinity;
  let maxY = -Infinity;


  trackMapData.tracks.forEach(
    function(track) {

      const pose =
        getTrackPose(track);

      if (!pose) {

        return;

      }


      minX =
        Math.min(
          minX,
          pose.entranceAX,
          pose.exitAX
        );

      maxX =
        Math.max(
          maxX,
          pose.entranceAX,
          pose.exitAX
        );

      minY =
        Math.min(
          minY,
          pose.entranceAY,
          pose.exitAY
        );

      maxY =
        Math.max(
          maxY,
          pose.entranceAY,
          pose.exitAY
        );

    }
  );


  if (
    !Number.isFinite(minX) ||
    !Number.isFinite(maxX) ||
    !Number.isFinite(minY) ||
    !Number.isFinite(maxY)
  ) {

    return;

  }


  const worldWidth =
    Math.max(
      maxX - minX,
      1
    );

  const worldHeight =
    Math.max(
      maxY - minY,
      1
    );


  const availableWidth =
    MAP_WIDTH -
    MAP_PADDING * 2;


  const availableHeight =
    MAP_HEIGHT -
    MAP_PADDING * 2;


  const scaleX =
    availableWidth /
    worldWidth;


  const scaleY =
    availableHeight /
    worldHeight;


  mapTransform.minX =
    minX;

  mapTransform.minY =
    minY;

  mapTransform.scale =
    Math.min(
      scaleX,
      scaleY
    );

}


// ============================================================
// DRAW ENTIRE TRACK MAP
// ============================================================

function drawTrackMap() {

  const svg =
    document.getElementById(
      "trackMapSvg"
    );

  svg.innerHTML = "";


  if (
    !trackMapData ||
    !Array.isArray(
      trackMapData.tracks
    )
  ) {

    return;

  }


  // Connections behind tracks.

  drawConnections(svg);


  // Draw tracks using backend geometry.

  trackMapData.tracks.forEach(
    function(track, index) {

      drawTrack(
        svg,
        track,
        index
      );

    }
  );

}


// ============================================================
// FIND CONNECTED TAG
// ============================================================

function findConnectedTag(
  track,
  targetTrackId
) {

  if (
    !Array.isArray(
      track.tags
    )
  ) {

    return null;

  }


  for (
    let i = 0;
    i < track.tags.length;
    i++
  ) {

    const connectedId =
      Number(
        track.tags[i].connectedTrack
      );


    if (
      Number.isFinite(
        connectedId
      ) &&
      connectedId ===
        targetTrackId
    ) {

      return track.tags[i];

    }

  }


  return null;

}


// ============================================================
// GET TAG WORLD POSITION
//
// Unlike the old version, tags are no longer positioned
// using hard-coded local geometry.
//
// The backend gives us the actual entrance and exitA
// coordinates.
// ============================================================

function getTagWorldPosition(
  track,
  tag
) {
  // TODO diff way of associating specific track pose with tag
  const pose =
    getTrackPose(track);

  if (!pose || !tag) {

    return null;

  }


  const name =
    String(
      tag.name || ""
    ).toLowerCase();


  if (
    name === "entrancea"
  ) {

    return {
      x: pose.entranceAX,
      y: pose.entranceAY
    };

  }


  if (
    name === "entranceb"
  ) {

    return {
      x: pose.entranceBX,
      y: pose.entranceBY
    };

  }


  if (
    name === "exita"
  ) {

    return {
      x: pose.exitAX,
      y: pose.exitAY
    };

  }


  if (
    name === "exitb"
  ) {

    return {
      x: pose.exitBX,
      y: pose.exitBY
    };

  }


  return null;

}


// ============================================================
// DRAW CONNECTIONS
// ============================================================

function drawConnections(svg) {

  trackMapData.tracks.forEach(
    function(track, trackIndex) {

      if (
        !Array.isArray(
          track.tags
        )
      ) {

        return;

      }


      track.tags.forEach(
        function(tag) {

          const connectedId =
            Number(
              tag.connectedTrack
            );


          if (
            !Number.isFinite(
              connectedId
            ) ||
            connectedId < 0 ||
            connectedId >=
              trackMapData.tracks.length
          ) {

            return;

          }


          // Only draw each connection once.

          if (
            trackIndex >
            connectedId
          ) {

            return;

          }


          const otherTrack =
            trackMapData.tracks[
              connectedId
            ];


          if (!otherTrack) {

            return;

          }


          const aWorld =
            getTagWorldPosition(
              track,
              tag
            );


          if (!aWorld) {

            return;

          }


          const otherTag =
            findConnectedTag(
              otherTrack,
              trackIndex
            );


          let bWorld = null;


          if (otherTag) {

            bWorld =
              getTagWorldPosition(
                otherTrack,
                otherTag
              );

          }


          // If the opposite tag cannot be resolved,
          // connect to the other track's entrance.

          if (!bWorld) {

            const otherPose =
              getTrackPose(
                otherTrack
              );

            if (otherPose) {

              bWorld = {

                x:
                  otherPose.entranceAX,

                y:
                  otherPose.entranceAY

              };

            }

          }


          if (!bWorld) {

            return;

          }


          const a =
            worldToMap(
              aWorld.x,
              aWorld.y
            );


          const b =
            worldToMap(
              bWorld.x,
              bWorld.y
            );


          drawLine(
            svg,
            a.x,
            a.y,
            b.x,
            b.y,
            "mapConnection"
          );

        }
      );

    }
  );

}


// ============================================================
// DRAW TRACK
//
// Position and orientation come directly from the backend.
// ============================================================

function drawTrack(
  svg,
  track,
  index
) {

  const pose =
    getTrackPose(track);


  if (!pose) {

    return;

  }


  const entrance =
    worldToMap(
      pose.entranceAX,
      pose.entranceAY
    );


  const exitA =
    worldToMap(
      pose.exitAX,
      pose.exitAY
    );


  const group =
    svgElement(
      "g",
      {
        "data-track-index":
          index
      }
    );


  svg.appendChild(
    group
  );


  const type =
    String(
      track.type || ""
    ).toUpperCase();


  switch (type) {

    case "STRAIGHT":

    case "BUMPER":

      drawBackendStraight(
        group,
        entrance,
        exitA,
        track
      );

      break;


    case "CURVED":

      drawBackendCurve(
        group,
        track,
        pose
      );

      break;


    case "SWITCH":

      drawBackendSwitch(
        group,
        track,
        pose
      );

      break;


    case "CROSS":

      drawBackendCross(
        group,
        track,
        pose
      );

      break;


    default:

      drawBackendStraight(
        group,
        entrance,
        exitA,
        track
      );

      break;

  }


  drawTags(
    group,
    track
  );


  drawTrackLabel(
    group,
    track,
    index,
    entrance,
    exitA
  );

}


// ============================================================
// DRAW BACKEND STRAIGHT
// ============================================================

function drawBackendStraight(
  group,
  entrance,
  exitA,
  track
) {

  drawLine(
    group,
    entrance.x,
    entrance.y,
    exitA.x,
    exitA.y,
    "mapTrack"
  );


  drawLine(
    group,
    entrance.x,
    entrance.y,
    exitA.x,
    exitA.y,
    "mapTrackCenter"
  );


  if (
    String(
      track.type || ""
    ).toUpperCase() ===
    "BUMPER"
  ) {

    const pose =
      getTrackPose(track);

    if (!pose) {
      return;
    }


    const heading =
      Number(
        pose.exitAHeading
      ) || 0;


    const radians =
      heading *
      Math.PI /
      180;


    const dx =
      Math.sin(radians) *
      15;


    const dy =
      -Math.cos(radians) *
      15;


    const bumper =
      svgElement(
        "line",
        {
          x1: pose.exitAX,
          y1: pose.exitAY - dy,
          x2: pose.exitAX + dx,
          y2: pose.exitAY + dy,
          stroke: "#b00020",
          "stroke-width": 8
        }
      );


    const p =
      worldToMap(
        pose.exitAX,
        pose.exitAY
      );


    // Rebuild bumper in map coordinates.

    bumper.setAttribute(
      "x1",
      p.x - dx
    );

    bumper.setAttribute(
      "y1",
      p.y - dy
    );

    bumper.setAttribute(
      "x2",
      p.x + dx
    );

    bumper.setAttribute(
      "y2",
      p.y + dy
    );


    group.appendChild(
      bumper
    );

  }

}


// ============================================================
// DRAW BACKEND CURVE
//
// The backend provides entrance/exitA positions and headings.
// Use those values to reconstruct the actual circular arc.
// ============================================================

function drawBackendCurve(
  group,
  track,
  pose
) {

  const radius =
    Number(
      track.radius
    );


  if (
    !Number.isFinite(radius) ||
    radius <= 0
  ) {

    const a =
      worldToMap(
        pose.entranceAX,
        pose.entranceAY
      );

    const b =
      worldToMap(
        pose.exitAX,
        pose.exitAY
      );


    drawLine(
      group,
      a.x,
      a.y,
      b.x,
      b.y,
      "mapTrack"
    );


    drawLine(
      group,
      a.x,
      a.y,
      b.x,
      b.y,
      "mapTrackCenter"
    );

    return;

  }


  const entranceAHeading =
    Number(
      pose.entranceAHeading
    ) || 0;


  const exitAHeading =
    Number(
      pose.exitAHeading
    ) || 0;


  // The backend uses:
  //
  // direction == true:
  //   center is 90 degrees left
  //
  // direction == false:
  //   center is 90 degrees right

  const centerHeading =
    track.direction
      ? entranceAHeading - 90
      : entranceAHeading + 90;


  const centerRadians =
    centerHeading *
    Math.PI /
    180;


  const centerWorld = {

    x:
      pose.entranceAX +
      Math.cos(
        centerRadians
      ) *
      radius,

    y:
      pose.entranceAY +
      Math.sin(
        centerRadians
      ) *
      radius

  };


  const startAngle =
    Math.atan2(
      pose.entranceAY -
      centerWorld.y,

      pose.entranceAX -
      centerWorld.x
    );


  const endAngle =
    Math.atan2(
      pose.exitAY -
      centerWorld.y,

      pose.exitAX -
      centerWorld.x
    );


  const center =
    worldToMap(
      centerWorld.x,
      centerWorld.y
    );


  const start =
    worldToMap(
      pose.entranceAX,
      pose.entranceAY
    );


  const end =
    worldToMap(
      pose.exitAX,
      pose.exitAY
    );


  // SVG uses the transformed radius.

  const svgRadius =
    radius *
    mapTransform.scale;


  let delta =
    endAngle -
    startAngle;


  // Keep the direction consistent with the backend.

  if (track.direction) {

    while (delta > 0) {
      delta -= Math.PI * 2;
    }

  }

  else {

    while (delta < 0) {
      delta += Math.PI * 2;
    }

  }


  const largeArc =
    Math.abs(delta) >
    Math.PI
      ? 1
      : 0;


  const sweep =
    track.direction
      ? 0
      : 1;


  const d =
    "M " +
    start.x +
    " " +
    start.y +

    " A " +
    svgRadius +
    " " +
    svgRadius +
    " 0 " +
    largeArc +
    " " +
    sweep +
    " " +
    end.x +
    " " +
    end.y;


  drawPath(
    group,
    d,
    "mapTrack"
  );


  drawPath(
    group,
    d,
    "mapTrackCenter"
  );

}


// ============================================================
// DRAW SWITCH
//
// The backend currently exports the calculated A entrance
// and A exitA pose. The branch geometry is therefore drawn
// relative to that backend-calculated centerline.
// ============================================================

function drawBackendSwitch(
  group,
  track,
  pose
) {
  const entrance = worldToMap(
    pose.entranceAX,
    pose.entranceAY
  );

  const exitA = worldToMap(
    pose.exitAX,
    pose.exitAY
  );

  const exitB = worldToMap(
    pose.exitBX,
    pose.exitBY
  );

  console.log(pose);
  // ---------------------------------------------------------
  // exitA A - straight
  // ---------------------------------------------------------
  drawLine(
    group,
    entrance.x,
    entrance.y,
    exitA.x,
    exitA.y,
    "mapTrack"
  );

  drawLine(
    group,
    entrance.x,
    entrance.y,
    exitA.x,
    exitA.y,
    "mapTrackCenter"
  );


  // ---------------------------------------------------------
  // exitA B - branch
  // ---------------------------------------------------------
  drawLine(
    group,
    entrance.x,
    entrance.y,
    exitB.x,
    exitB.y,
    "mapTrack"
  );

  drawLine(
    group,
    entrance.x,
    entrance.y,
    exitB.x,
    exitB.y,
    "mapTrackCenter"
  );
}

// ============================================================
// DRAW CROSS
//
// Same principle as SWITCH: use the backend coordinates
// rather than creating a second frontend coordinate system.
// ============================================================

function drawBackendCross(
  group,
  track,
  pose
) {

  const entrance =
    worldToMap(
      pose.entranceAX,
      pose.entranceAY
    );


  const exitA =
    worldToMap(
      pose.exitAX,
      pose.exitAY
    );


  drawLine(
    group,
    entrance.x,
    entrance.y,
    exitA.x,
    exitA.y,
    "mapTrack"
  );


  drawLine(
    group,
    entrance.x,
    entrance.y,
    exitA.x,
    exitA.y,
    "mapTrackCenter"
  );

}


// ============================================================
// DRAW TAGS
//
// Tags are located from backend pose data.
// ============================================================

function drawTags(
  group,
  track
) {

  if (
    !Array.isArray(
      track.tags
    )
  ) {

    return;

  }


  track.tags.forEach(
    function(tag) {

      const world =
        getTagWorldPosition(
          track,
          tag
        );


      if (!world) {

        return;

      }


      const point =
        worldToMap(
          world.x,
          world.y
        );


      const connected =
        tag.connected === true ||
        (
          Number.isFinite(
            Number(
              tag.connectedTrack
            )
          ) &&
          Number(
            tag.connectedTrack
          ) >= 0
        );


      const circle =
        svgElement(
          "circle",
          {
            cx: point.x,
            cy: point.y,
            r: 8,

            class:
              connected
                ? "mapTag"
                : "mapTag unconnected"
          }
        );


      group.appendChild(
        circle
      );


      const label =
        svgElement(
          "text",
          {
            x: point.x,
            y: point.y - 12,
            "text-anchor": "middle",
            class: "mapTagLabel"
          }
        );


      label.textContent =
        tag.name || "";


      group.appendChild(
        label
      );

    }
  );

}


// ============================================================
// TRACK LABEL
// ============================================================

function drawTrackLabel(
  group,
  track,
  index,
  entrance,
  exitA
) {

  const x =
    (
      entrance.x +
      exitA.x
    ) / 2;


  const y =
    (
      entrance.y +
      exitA.y
    ) / 2;


  const label =
    svgElement(
      "text",
      {
        x: x,
        y: y - 15,
        "text-anchor": "middle",
        class: "mapTrackLabel"
      }
    );


  label.textContent =
    "#" +
    index +
    " " +
    String(
      track.type || ""
    );


  group.appendChild(
    label
  );

}


// ============================================================
// RESET MAP
// ============================================================

function resetMapView() {

  calculateMapTransform();

  drawTrackMap();

}


// ============================================================
// INITIAL TRACK MAP
// ============================================================

refreshTrackMap();


setInterval(
  refreshTrackMap,
  2000
);


// ============================================================
// MOTOR SPEED
// ============================================================

function updateSpeed(value) {

  document.getElementById(
    "speedValue"
  ).innerHTML =
    value;


  if (
    value > 0
  ) {

    document.getElementById(
      "direction"
    ).innerHTML =
      "FORWARD";

  }

  else if (
    value < 0
  ) {

    document.getElementById(
      "direction"
    ).innerHTML =
      "REVERSE";

  }

  else {

    document.getElementById(
      "direction"
    ).innerHTML =
      "STOPPED";

  }


  const xhr =
    new XMLHttpRequest();


  xhr.open(
    "GET",
    "/slider?value=" +
    encodeURIComponent(value),
    true
  );


  xhr.send();

}


// ============================================================
// RAMP
// ============================================================

function updateRamp(value) {

  document.getElementById(
    "rampValue"
  ).innerHTML =
    parseFloat(value)
      .toFixed(1);


  const xhr =
    new XMLHttpRequest();


  xhr.open(
    "GET",
    "/ramp?ramp=" +
    encodeURIComponent(value),
    true
  );


  xhr.send();

}


// ============================================================
// LED
// ============================================================

function toggleLED(
  led
) {

  const button =
    document.getElementById(
      "led" +
      led +
      "Button"
    );


  const isOn =
    button.classList.contains(
      "on"
    );


  const newState =
    isOn
      ? 0
      : 1;


  const xhr =
    new XMLHttpRequest();


  xhr.open(
    "GET",
    "/led" +
    led +
    "?state=" +
    newState,
    true
  );


  xhr.onload =
    function() {

      if (
        xhr.status === 200
      ) {

        updateState();

      }

    };


  xhr.send();

}


// ============================================================
// AUDIO
// ============================================================

function toggleAudio() {

  const xhr =
    new XMLHttpRequest();


  xhr.open(
    "GET",
    "/audio",
    true
  );


  xhr.onload =
    function() {

      if (
        xhr.status !== 200
      ) {

        return;

      }


      updateState();

    };


  xhr.send();

}


// ============================================================
// AUTO
// ============================================================

function toggleAuto() {

  const button =
    document.getElementById(
      "autoButton"
    );


  const isOn =
    button.classList.contains(
      "on"
    );


  const newState =
    isOn
      ? 0
      : 1;


  const xhr =
    new XMLHttpRequest();


  xhr.open(
    "GET",
    "/auto?state=" +
    newState,
    true
  );


  xhr.onload =
    function() {

      if (
        xhr.status === 200
      ) {

        updateState();

      }

    };


  xhr.send();

}


// ============================================================
// CONTROLLER PAIRING
// ============================================================

function togglePairing() {

  const xhr =
    new XMLHttpRequest();


  xhr.open(
    "GET",
    "/pairing/toggle",
    true
  );


  xhr.onload =
    function() {

      refreshControllers();

    };


  xhr.send();

}


// ============================================================
// REFRESH CONTROLLERS
// ============================================================

function refreshControllers() {

  const xhr =
    new XMLHttpRequest();


  xhr.open(
    "GET",
    "/controllers",
    true
  );


  xhr.onload =
    function() {

      if (
        xhr.status !== 200
      ) {

        return;

      }


      let data;


      try {

        data =
          JSON.parse(
            xhr.responseText
          );

      }

      catch (
        error
      ) {

        console.error(
          "Invalid controller JSON:",
          error
        );

        return;

      }


      document.getElementById(
        "pairStatus"
      ).innerHTML =
        data.pairing
          ? "OPEN"
          : "CLOSED";


      document.getElementById(
        "activeController"
      ).innerHTML =
        data.active;


      const container =
        document.getElementById(
          "controllers"
        );


      container.innerHTML = "";


      if (
        !Array.isArray(
          data.controllers
        )
      ) {

        return;

      }


      data.controllers.forEach(
        function(
          controller,
          index
        ) {

          const div =
            document.createElement(
              "div"
            );


          div.className =
            controller.active
              ? "controller active"
              : "controller";


          const name =
            document.createElement(
              "b"
            );


          name.textContent =
            controller.name ||
            "";


          div.appendChild(
            name
          );


          div.appendChild(
            document.createElement(
              "br"
            )
          );


          const mac =
            document.createTextNode(
              controller.mac ||
              ""
            );


          div.appendChild(
            mac
          );


          div.appendChild(
            document.createElement(
              "br"
            )
          );


          const lastSeen =
            document.createTextNode(
              "Last seen: " +
              controller.lastSeen +
              " ms ago"
            );


          div.appendChild(
            lastSeen
          );


          div.appendChild(
            document.createElement(
              "br"
            )
          );


          const button =
            document.createElement(
              "button"
            );


          button.className =
            "smallButton danger";


          button.textContent =
            "REMOVE";


          button.onclick =
            function() {

              removeController(
                index
              );

            };


          div.appendChild(
            button
          );


          container.appendChild(
            div
          );

        }
      );

    };


  xhr.send();

}


// ============================================================
// REMOVE CONTROLLER
// ============================================================

function removeController(
  index
) {

  if (
    !confirm(
      "Remove this controller?"
    )
  ) {

    return;

  }


  const xhr =
    new XMLHttpRequest();


  xhr.open(
    "GET",
    "/controller/remove?slot=" +
    encodeURIComponent(index),
    true
  );


  xhr.onload =
    function() {

      refreshControllers();

    };


  xhr.send();

}


// ============================================================
// SERIAL TERMINAL
// ============================================================

function updateSerialTerminal() {

  const xhr =
    new XMLHttpRequest();


  xhr.open(
    "GET",
    "/serial",
    true
  );


  xhr.onload =
    function() {

      if (
        xhr.status !== 200
      ) {

        return;

      }


      const terminal =
        document.getElementById(
          "serialTerminal"
        );


      const distanceFromBottom =
        terminal.scrollHeight -
        terminal.scrollTop -
        terminal.clientHeight;


      const shouldAutoScroll =
        distanceFromBottom < 50;


      let text =
        xhr.responseText;


      text =
        text
          .replace(
            /&/g,
            "&amp;"
          )
          .replace(
            /</g,
            "&lt;"
          )
          .replace(
            />/g,
            "&gt;"
          )
          .replace(
            /\r\n/g,
            "\n"
          )
          .replace(
            /\r/g,
            "\n"
          )
          .replace(
            /\n/g,
            "<br>"
          );


      terminal.innerHTML =
        text;


      if (
        shouldAutoScroll
      ) {

        terminal.scrollTop =
          terminal.scrollHeight;

      }

    };


  xhr.send();

}


// ============================================================
// CLEAR SERIAL
// ============================================================

function clearSerial() {

  const xhr =
    new XMLHttpRequest();


  xhr.open(
    "GET",
    "/serial/clear",
    true
  );


  xhr.send();


  document.getElementById(
    "serialTerminal"
  ).textContent =
    "";

}


// ============================================================
// STATE BUTTON HELPERS
// ============================================================

function updateLEDButton(
  led,
  state
) {

  const button =
    document.getElementById(
      "led" +
      led +
      "Button"
    );


  if (state) {

    button.classList.remove(
      "off"
    );


    button.classList.add(
      "on"
    );


    button.innerHTML =
      "LED " +
      led +
      " ON";

  }

  else {

    button.classList.remove(
      "on"
    );


    button.classList.add(
      "off"
    );


    button.innerHTML =
      "LED " +
      led +
      " OFF";

  }

}


// ============================================================
// AUDIO BUTTON
// ============================================================

function updateAudioButton(
  state
) {

  const button =
    document.getElementById(
      "audioButton"
    );


  if (state) {

    button.classList.remove(
      "off"
    );


    button.classList.add(
      "on"
    );


    button.innerHTML =
      "AUDIO ON";

  }

  else {

    button.classList.remove(
      "on"
    );


    button.classList.add(
      "off"
    );


    button.innerHTML =
      "AUDIO OFF";

  }

}


// ============================================================
// AUTO BUTTON
// ============================================================

function updateAutoButton(
  state
) {

  const button =
    document.getElementById(
      "autoButton"
    );


  if (state) {

    button.classList.remove(
      "off"
    );


    button.classList.add(
      "on"
    );


    button.innerHTML =
      "AUTO ON";

  }

  else {

    button.classList.remove(
      "on"
    );


    button.classList.add(
      "off"
    );


    button.innerHTML =
      "AUTO OFF";

  }

}


// ============================================================
// GET CURRENT ESP32 STATE
// ============================================================

function updateState() {

  const xhr =
    new XMLHttpRequest();


  xhr.open(
    "GET",
    "/state",
    true
  );


  xhr.onload =
    function() {

      if (
        xhr.status !== 200
      ) {

        return;

      }


      let data;


      try {

        data =
          JSON.parse(
            xhr.responseText
          );

      }

      catch (
        error
      ) {

        console.error(
          "Invalid state JSON:",
          error
        );

        return;

      }


      updateLEDButton(
        1,
        data.led1
      );


      updateLEDButton(
        2,
        data.led2
      );


      updateAudioButton(
        data.audio
      );


      updateAutoButton(
        data.auto
      );


      if (
        data.speed !== undefined
      ) {

        document.getElementById(
          "speedSlider"
        ).value =
          data.speed;


        document.getElementById(
          "speedValue"
        ).innerHTML =
          data.speed;


        if (
          data.speed > 0
        ) {

          document.getElementById(
            "direction"
          ).innerHTML =
            "FORWARD";

        }

        else if (
          data.speed < 0
        ) {

          document.getElementById(
            "direction"
          ).innerHTML =
            "REVERSE";

        }

        else {

          document.getElementById(
            "direction"
          ).innerHTML =
            "STOPPED";

        }

      }


      if (
        data.ramp !== undefined
      ) {

        const rampSeconds =
          Number(data.ramp) /
          1000;


        document.getElementById(
          "rampSlider"
        ).value =
          rampSeconds;


        document.getElementById(
          "rampValue"
        ).innerHTML =
          rampSeconds.toFixed(1);

      }

    };


  xhr.send();

}


// ============================================================
// PERIODIC UPDATES
// ============================================================

setInterval(
  updateSerialTerminal,
  250
);


setInterval(
  refreshControllers,
  1000
);


setInterval(
  updateState,
  250
);


// ============================================================
// INITIAL UPDATES
// ============================================================

updateSerialTerminal();

refreshControllers();

updateState();

</script>

</body>

</html>

)WEBPAGE";

// ============================================================
// GENERATE WEB PAGE
// ============================================================

String getIndexPage() {
  Serial.print("Free heap BEFORE page: ");
  Serial.println(ESP.getFreeHeap());

  String page = String(index_html);

  Serial.print("Free heap AFTER page: ");
  Serial.println(ESP.getFreeHeap());

  return page;
}