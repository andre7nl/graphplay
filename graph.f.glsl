uniform sampler2D mytexture;
varying vec2 texpos;
varying vec4 f_color;
uniform float sprite;

void main(void) {
if (sprite > 1.0) {
// text
    gl_FragColor = vec4(1, 1, 1, texture2D(mytexture, texpos).a) * f_color;
}
else {
// graph
	// if (sprite > 1.0)
	// 	gl_FragColor = texture2D(mytexture, gl_PointCoord) * f_color;
	// else
	gl_FragColor = f_color;
}
}
