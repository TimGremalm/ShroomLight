var shroomws = (function(host) {
	// Some local variables. I declare them up top so that we can add them to
	// the returned object later. It's like local variables on a class, except
	// this isn't a class.
	
	let shrooms = {};
	let dom_shrooms = document.getElementById("shrooms");

	const LIGHTMODE_IDLE = 0;
	const LIGHTMODE_OFF = 1;
	const LIGHTMODE_SOLID = 2;
	const LIGHTMODE_WAVE_LIGHT = 3;
	const LIGHTMODE_WAVE_MEDIUM = 4;
	const LIGHTMODE_WAVE_HARD = 5;

	// Make sure to use SVG name space to let the browser know it isn't HTML any more
	let xmlns = "http://www.w3.org/2000/svg";
	let svg = document.createElementNS(xmlns, "svg");
	svg.setAttributeNS(null, "viewBox", "-1500 -1000 3000 2000");
	svg.setAttributeNS(null, "width", "100%");
	svg.setAttributeNS(null, "height", "100%");
	let svgg = document.createElementNS(xmlns, "g");
	svgg.setAttributeNS(null, "transform", "rotate(30)");
	svgg.setAttributeNS(null, "fill", "gray");
	svgg.setAttributeNS(null, "stroke", "black");
	svg.appendChild(svgg);
	let dom_shroommap = document.getElementById("shroommap");
	dom_shroommap.appendChild(svg);

	let hexes = makeHexagonalShape(20);
	for (hex of hexes) {
		draw_hex_svg(svgg, hex);
	}

	// Let's connect to the websocket..
	console.log("connecting to ws on %s...", host);
	const ws = new WebSocket(host);
	ws.onopen = function() {
		console.log("Websocket opened, query shrooms for information");
		ws.send("information");
	};

	let shroom_setLightMode = function(mac, mode) {
		ws.send("LIGHTMODE " + mac + " " + mode);
	}

	ws.onmessage = function (evt) { 
		const received_msg = evt.data;
		console.log("Received websocket message: %s", received_msg);
		
		// Parse 
		const data_parts = received_msg.split(" ");
		
		// Dispatch on first part of message
		if (data_parts[0] === "Shroom") {
			// This is a status update from one of the shrooms - the message should
			// look like this:
			//   Shroom <mac> Version <version> Grid <x> <y> <z>
			const mac = data_parts[1];
			const version = data_parts[3];
			const x = parseInt(data_parts[5]);
			const y = parseInt(data_parts[6]);
			const z = parseInt(data_parts[7]);

			console.log("Shroom %s (running version %d) is at (%d, %d, %d).",
				mac, version, x, y, z
			);

			// Add (or update) this device in the shrooms hash table
			if (undefined === shrooms[mac]) {
				shrooms[mac] = {
					version: version,
					x: x,
					y: y,
					z: z,
					shroomhex: new Array(7)
				}
				shrooms[mac].shroomhex[0] = new Hex(x+0, y+0, z+0);  // First shroom
				shrooms[mac].shroomhex[1] = new Hex(x+0, y-1, z+1);  // Shorom around first, offset
				shrooms[mac].shroomhex[2] = new Hex(x+1, y-1, z+0);
				shrooms[mac].shroomhex[3] = new Hex(x+1, y+0, z-1);
				shrooms[mac].shroomhex[4] = new Hex(x+0, y+1, z-1);
				shrooms[mac].shroomhex[5] = new Hex(x-1, y+1, z+0);
				shrooms[mac].shroomhex[6] = new Hex(x-1, y+0, z+1);
				// This is a new device; let's create 7 SVG elements for it for each shroom in device
				for (i=0; i< shrooms[mac].shroomhex.length; i++) {
					// Generate color for Shoroom Light based on it's MAC address
					var myrng = new Math.seedrandom(mac);
					var rgb = hslToRgb(myrng.quick(), 0.3, 0.8);
					var color = getColorStringFromRGB(rgb);
					if (i == 0) {
						draw_hex_svg(svgg, shrooms[mac].shroomhex[i], 'Shroom'+mac+'_'+i, 'shroomlight', color, mac, version);
					} else {
						draw_hex_svg(svgg, shrooms[mac].shroomhex[i], 'Shroom'+mac+'_'+i, 'shroomlight', color);
					}
				}
			} else {
				//Only modify if corrdinates is changed
				if (shrooms[mac].version == version && shrooms[mac].x == x && shrooms[mac].y == y && shrooms[mac].z == z) {
					console.log(mac+" no changes")
				} else {
					shrooms[mac].version = version;
					shrooms[mac].x = x;
					shrooms[mac].y = y;
					shrooms[mac].z = z;
					shrooms[mac].shroomhex[0] = new Hex(x+0, y+0, z+0);  // First shroom
					shrooms[mac].shroomhex[1] = new Hex(x+0, y-1, z+1);  // Shorom around first, offset
					shrooms[mac].shroomhex[2] = new Hex(x+1, y-1, z+0);
					shrooms[mac].shroomhex[3] = new Hex(x+1, y+0, z-1);
					shrooms[mac].shroomhex[4] = new Hex(x+0, y+1, z-1);
					shrooms[mac].shroomhex[5] = new Hex(x-1, y+1, z+0);
					shrooms[mac].shroomhex[6] = new Hex(x-1, y+0, z+1);
					// Move existing svg graphics for eventual new position
					for (i=0; i< shrooms[mac].shroomhex.length; i++) {
						xy = pointy_hex_to_pixel(100, shrooms[mac].shroomhex[i]);
						gPos = document.getElementById('Shroom'+mac+'_'+i);
						gPos.setAttributeNS(null, "transform", "translate(" + xy[0] + "," + xy[1] + ")");
						console.log(mac+" - new address x "+xy[0]+" new y "+xy[1]);
						// Change x, y, z coordinate text
						qLabel = gPos.getElementsByClassName("q-coord")[0];
						qLabel.textContent = "x "+shrooms[mac].shroomhex[i].q;
						rLabel = gPos.getElementsByClassName("r-coord")[0];
						rLabel.textContent = "y "+shrooms[mac].shroomhex[i].r;
						sLabel = gPos.getElementsByClassName("s-coord")[0];
						sLabel.textContent = "z "+shrooms[mac].shroomhex[i].s;
						if (i == 0) {  // Only set version lebel on center hex in Shroom Light
							versionLabel = gPos.getElementsByClassName("labelversion")[0];
							versionLabel.textContent = shrooms[mac].version;
						}
					}
				}
			}

			// Add (or update) this device in the shroom table on the website
			dom_shroom = document.getElementById(mac);
			if (null === dom_shroom) {
				// Shroom is not in the table yet - let's create it.
				const el = document.createElement("div");
				el.id = mac;
				el.classList += "shroom";

				const el_header = document.createElement("span");
				el_header.classList += "mac";
				el_header.innerText = mac;
				el.appendChild(el_header);

				const el_version = document.createElement("span");
				el_version.classList += "version";
				el_version.innerText = version;
				el.appendChild(el_version);

				const el_x = document.createElement("span");
				el_x.classList += "x";
				el_x.innerText = x;
				el.appendChild(el_x);

				const el_y = document.createElement("span");
				el_y.classList += "y";
				el_y.innerText = y;
				el.appendChild(el_y);

				const el_z = document.createElement("span");
				el_z.classList += "z";
				el_z.innerText = z;
				el.appendChild(el_z);


				// Add buttons (refactor this)
				let buttons = {
					"idle": LIGHTMODE_IDLE,
					"off": LIGHTMODE_OFF,
					"solid": LIGHTMODE_SOLID,
					"wave-light": LIGHTMODE_WAVE_LIGHT,
					"wave-medium": LIGHTMODE_WAVE_MEDIUM,
					"wave-hard": LIGHTMODE_WAVE_HARD,
				};
				Object.keys(buttons).map(function(key) {
					const el_b = document.createElement("button");
					el_b.innerText = key;
					el_b.onclick = function() {
						shroom_setLightMode(mac, buttons[key]);
					};
					el.appendChild(el_b);
				});

				dom_shrooms.appendChild(el);
			}
		} else if (data_parts[0] === "SHROOM") {
			// SHROOM <mac-origin> <hops> <wave-generation> <x> <y> <z>
		} else if (data_parts[0] === "information") {
			// Not interested in this currently, just
			// making sure every case is caught.
		} else if (data_parts[0] === "SETGRID") {
			// An address is changed, ask Shroom Lights to report information
			ws.send("information");
		} else {
			console.log("uncaught incoming command: %s", evt.data);
		}
	};

	ws.onclose = function() { 
		console.log("Connection is closed..."); 
	};

	return {
		ws: ws,
		information: function() {
			ws.send("information");
		},
		shrooms: shrooms
	};
});

function pointy_hex_to_pixel(size, hex) {
	var x = size * (Math.sqrt(3) * hex.q + Math.sqrt(3)/2 * hex.r);
	var y = size * (3/2 * hex.r);
	return [x,y];
}

function cube_to_axial(hex) {
	var q = hex.x
	var r = hex.z
	return new Hex(q, r)
}

function axial_to_cube(hex) {
	var x = hex.q
	var y = hex.r
	var z = -x-y
	return new Hex(x, y, z)
}

function toPaddedHexString(num, len) {
	str = num.toString(16);
	return "0".repeat(len - str.length) + str;
}

function getColorStringFromRGB(rgbarray) {
	var colorout = "#"
	colorout += toPaddedHexString(parseInt(rgbarray[0]), 2);
	colorout += toPaddedHexString(parseInt(rgbarray[1]), 2);
	colorout += toPaddedHexString(parseInt(rgbarray[2]), 2);
	return colorout;
}

// svg is an svg element, hex is an object with q and r keys
function draw_hex_svg(svg, hex, gid='', gclass='hexgrid', gbgcolor='#ddd', glabelmac='', glabelversion='') {
	cubecoords = axial_to_cube(hex);
	let xmlns = "http://www.w3.org/2000/svg";
	xy = pointy_hex_to_pixel(100, hex);
	let gPos = document.createElementNS(xmlns, "g");
	gPos.setAttributeNS(null, "transform", "translate(" + xy[0] + "," + xy[1] + ")");

	if (gid === "") {
		gPos.setAttributeNS(null, "id", cubecoords.q + "_" + cubecoords.r + "_" + cubecoords.s);
	} else {
		gPos.setAttributeNS(null, "id", gid);
	}
	gPos.setAttributeNS(null, "class", gclass);
	gPos.setAttributeNS(null, "fill", gbgcolor);
	let gPol = document.createElementNS(xmlns, "g");
	gPol.setAttributeNS(null, "transform", "rotate(-30)");
	let poly = document.createElementNS(xmlns, "polygon");
	poly.setAttributeNS(null, "points", "100,0 50,-87 -50,-87 -100,-0 -50,87 50,87");

	// x, y, z labels
	let textCont = document.createElementNS(xmlns, "text");
	textCont.setAttributeNS(null, "transform", "rotate(0)");
	textCont.setAttributeNS(null, "font-size", "31");
	let tspanX = document.createElementNS(xmlns, "tspan");
	tspanX.setAttributeNS(null, "x", "-80");
	tspanX.setAttributeNS(null, "y", "-16");
	tspanX.setAttributeNS(null, "class", "q-coord");
	tspanX.textContent = "x " + cubecoords.q
	let tspanY = document.createElementNS(xmlns, "tspan");
	tspanY.setAttributeNS(null, "x", "8");
	tspanY.setAttributeNS(null, "y", "-16");
	tspanY.setAttributeNS(null, "class", "r-coord");
	tspanY.textContent = "y " + cubecoords.r
	let tspanZ = document.createElementNS(xmlns, "tspan");
	tspanZ.setAttributeNS(null, "x", "-30");
	tspanZ.setAttributeNS(null, "y", "60");
	tspanZ.setAttributeNS(null, "class", "s-coord");
	tspanZ.textContent = "z " + cubecoords.s
	let tspanMac = document.createElementNS(xmlns, "tspan");
	tspanMac.setAttributeNS(null, "x", "-90");
	tspanMac.setAttributeNS(null, "y", "20");
	tspanMac.setAttributeNS(null, "class", "labelmac");
	tspanMac.setAttributeNS(null, "fill", "black");
	tspanMac.textContent = glabelmac;
	let tspanVersion = document.createElementNS(xmlns, "tspan");
	tspanVersion.setAttributeNS(null, "x", "-30");
	tspanVersion.setAttributeNS(null, "y", "-50");
	tspanVersion.setAttributeNS(null, "class", "labelversion");
	tspanVersion.setAttributeNS(null, "fill", "black");
	tspanVersion.textContent = glabelversion;

	textCont.appendChild(tspanX);
	textCont.appendChild(tspanY);
	textCont.appendChild(tspanZ);
	textCont.appendChild(tspanMac);
	textCont.appendChild(tspanVersion);
	gPol.appendChild(poly);
	gPol.appendChild(textCont);
	gPos.appendChild(gPol);
	svg.appendChild(gPos);

	if (gclass === "hexgrid") {
		gPos.onclick = function(el) {
			//el.target.setAttributeNS(null, "fill", "green");
			console.log("click " + el.currentTarget.id);
			el.currentTarget.setAttributeNS(null, "fill", "green");
			//el.currentTarget.setAttributeNS(null, "transform", "translate(0, 0)");
		}
	}
	if (gclass === "shroomlight") {
	}

	gPos.onmouseenter = function(el) {
		//el.target.setAttributeNS(null, "fill", "green");
	}

	gPos.onmouseout = function(el) {
		//el.target.setAttributeNS(null, "fill", 'none');
	}
}

window.onload = function() {
	// Sanity checking
	if (!("WebSocket" in window)) {
		console.log("No websocket support!");
		return {};
	}

	// Local variables
	const wsaddress = "ws://" + location.hostname + ":9001/"

	window.shroomws = shroomws(wsaddress);
};

