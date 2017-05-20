#version 330

layout(triangles) in;
layout(triangle_strip, max_vertices = 12) out;




in vec3 normal[3];
in vec3 erosionData[3];
in vec4 fluxData[3];
in vec4 velocityData[3];


in vec3 wsInterpolatedEye[3];

uniform mat4 MVP;

// uniforms for phong lighting
uniform vec3 wsLight;
uniform vec3 lightColor;
uniform vec3 ambient;

out vec3 faceColor;
out vec4 fluxData_final;
out vec3 erosionData_final;
out vec4 velocityData_final;

void main()
{
	vec3 normal = normalize(cross(gl_in[1].gl_Position.xyz - gl_in[0].gl_Position.xyz,
							gl_in[2].gl_Position.xyz - gl_in[0].gl_Position.xyz));

	// normalColor
	vec3 color = normal * 0.5 + vec3(0.5);

	// make greyscale:
	float grey = ((29.9/100.0) * color.x) + ((58.7/100.0) * color.y) + ((11.4/100.0) * color.z);
	vec3 greyScale = vec3(grey);



	// phong data
	float lambertScalar = .4;
	vec3 lambertColor = vec3(.5, .5, .5);


	for (int i = 0; i < 3; i++) {
		gl_Position = MVP * (gl_in[i].gl_Position);
		faceColor = greyScale;

		erosionData_final = erosionData[i];
		fluxData_final = fluxData[i];
		velocityData_final = velocityData[i];
		EmitVertex();
	}
	EndPrimitive();

}