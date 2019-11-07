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
	svg.setAttributeNS(null, "viewBox", "-1000 -500 2000 1000");
	svg.setAttributeNS(null, "width", "100%");
	svg.setAttributeNS(null, "height", "100%");
	let svgg = document.createElementNS(xmlns, "g");
	svgg.setAttributeNS(null, "transform", "rotate(-30)");
	svgg.setAttributeNS(null, "fill", "gray");
	svgg.setAttributeNS(null, "stroke", "black");
	svg.appendChild(svgg);
	let dom_shroommap = document.getElementById("shroommap");
	dom_shroommap.appendChild(svg);

	let hexes = hexRing(1);
	for (hex of hexes) {
		draw_hex_svg(svgg, hex);
	}

	hexes = hexRing(2);
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
			const x = data_parts[5];
			const y = data_parts[6];
			const z = data_parts[7];

			console.log("Shroom %s (running version %d) is at (%d, %d, %d).",
				mac, version, x, y, z
			);

			// Add (or update) this device in the shrooms hash table
			if (undefined === shrooms[mac]) {
				// This is a new device; let's create a SVG element for it
				shrooms[mac] = {
					version: version,
					x: x,
					y: y,
					z: z,
					svg: null
				}
				//svg.appendChild();
			} else {
				shrooms[mac].version = version;
				shrooms[mac].x = x;
				shrooms[mac].y = y;
				//shrooms[mac].svg.setAttributeNS(null, "transform", "");
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

// svg is an svg element, hex is an object with q and r keys
function draw_hex_svg(svg, hex) {
	let xmlns = "http://www.w3.org/2000/svg";
	xy = pointy_hex_to_pixel(100, hex);
	let el = document.createElementNS(xmlns, "g");
	el.setAttributeNS(null, "transform", "translate(" + xy[0] + "," + xy[1] + ")");
	let elg = document.createElementNS(xmlns, "g");
	elg.setAttributeNS(null, "transform", "rotate(-30)");
	let poly = document.createElementNS(xmlns, "polygon");
	poly.setAttributeNS(null, "points", "100,0 50,-87 -50,-87 -100,-0 -50,87 50,87");
	elg.appendChild(poly);
	el.appendChild(elg);
	svg.appendChild(el);

	poly.onmouseenter = function(el) {
		el.target.parentElement.setAttributeNS(null, "fill", "green");
	}

	poly.onmouseout = function(el) {
		el.target.parentElement.setAttributeNS(null, "fill", "gray");
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

