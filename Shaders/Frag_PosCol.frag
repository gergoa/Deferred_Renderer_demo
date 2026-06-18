#version 430

// pipeline-ból bejövő per-fragment attribútumok 
in vec3 color;

// kimenő érték - a fragment színe 
out vec4 outputColor;

void main()
{
	outputColor = vec4( color, 1.0 );
}