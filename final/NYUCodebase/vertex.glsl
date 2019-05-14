attribute vec4 position;

uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

uniform vec2 offset;

void main()
{
	vec4 p = viewMatrix * modelMatrix  * position;
	gl_Position = projectionMatrix * p;
}