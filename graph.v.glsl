attribute vec4 coord;
uniform float xscale;
uniform float yscale;
uniform lowp float sprite;
varying vec4 f_color;
varying vec2 texpos;
uniform vec4 tx_color;

void main(void) {
if (sprite > 1.0) {
// text
  gl_Position = vec4(coord.xy, 0, 1);
  texpos = coord.zw;
  f_color = tx_color;
  }
else{
// graph
	gl_Position = vec4(2 * coord.x / xscale - 1.0, (2 * coord.y/1024 - 1.0) * yscale, 0, 1);
	f_color = vec4(coord.xy / xscale, 1, 1);
	gl_PointSize = max(1.0, sprite);
	}
}
