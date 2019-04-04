
uniform sampler2D texture0;

varying vec2 texCoordVar;

void main() {
	gl_FragColor += texture2D(texture0, texCoordVar);
}