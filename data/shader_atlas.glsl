flat basic.vs flat.fs
texture basic.vs texture.fs
skybox basic.vs skybox.fs
depth quad.vs depth.fs
multi basic.vs multi.fs
phong phong.vs phong.fs
phong_multipass_ambient phong.vs phong_multipass_ambient.fs
phong_multipass_light phong.vs phong_multipass_light.fs

compute test.cs

\test.cs
#version 430 core

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() 
{
	vec4 i = vec4(0.0);
}

\basic.vs

#version 330 core

in vec3 a_vertex;
in vec3 a_normal;
in vec2 a_coord;
in vec4 a_color;
in vec3 a_tangent;
in vec3 a_bitangent;

uniform vec3 u_camera_pos;

uniform mat4 u_model;
uniform mat4 u_viewprojection;

//this will store the color for the pixel shader
out vec3 v_position;
out vec3 v_world_position;
out vec3 v_normal;
out vec2 v_uv;
out vec4 v_color;
out vec3 v_tangent;
out vec3 v_bitangent;

uniform float u_time;

void main()
{	
	//calcule the normal in camera space (the NormalMatrix is like ViewMatrix but without traslation)
	v_normal = (u_model * vec4( a_normal, 0.0) ).xyz;
    v_tangent = (u_model * vec4( a_tangent, 0.0) ).xyz;
	v_bitangent = (u_model * vec4( a_bitangent, 0.0) ).xyz;
	
	//calcule the vertex in object space
	v_position = a_vertex;
	v_world_position = (u_model * vec4( v_position, 1.0) ).xyz;
	
	//store the color in the varying var to use it from the pixel shader
	v_color = a_color;

	//store the texture coordinates
	v_uv = a_coord;

	//calcule the position of the vertex using the matrices
	gl_Position = u_viewprojection * vec4( v_world_position, 1.0 );
}

\quad.vs

#version 330 core

in vec3 a_vertex;
in vec2 a_coord;
out vec2 v_uv;

void main()
{	
	v_uv = a_coord;
	gl_Position = vec4( a_vertex, 1.0 );
}


\flat.fs

#version 330 core

uniform vec4 u_color;

out vec4 FragColor;

void main()
{
	FragColor = u_color;
}


\texture.fs

#version 330 core

in vec3 v_position;
in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_color;

uniform vec4 u_color;
uniform sampler2D u_texture;
uniform float u_time;
uniform float u_alpha_cutoff;

uniform vec3 u_light_pos[10];
uniform vec3 u_light_color[10];
uniform float u_light_intensity[10];


out vec4 FragColor;

void main()
{
	vec2 uv = v_uv;
	vec4 color = u_color;
	color *= texture( u_texture, v_uv );

	if(color.a < u_alpha_cutoff)
		discard;

	FragColor = color;
}


\skybox.fs

#version 330 core

in vec3 v_position;
in vec3 v_world_position;

uniform samplerCube u_texture;
uniform vec3 u_camera_position;
out vec4 FragColor;

void main()
{
	vec3 E = v_world_position - u_camera_position;
	vec4 color = texture( u_texture, E );
	FragColor = color;
}


\multi.fs

#version 330 core

in vec3 v_position;
in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;

uniform vec4 u_color;
uniform sampler2D u_texture;
uniform float u_time;
uniform float u_alpha_cutoff;

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 NormalColor;

void main()
{
	vec2 uv = v_uv;
	vec4 color = u_color;
	color *= texture( u_texture, uv );

	if(color.a < u_alpha_cutoff)
		discard;

	vec3 N = normalize(v_normal);

	FragColor = color;
	NormalColor = vec4(N,1.0);
}


\depth.fs

#version 330 core

uniform vec2 u_camera_nearfar;
uniform sampler2D u_texture; //depth map
in vec2 v_uv;
out vec4 FragColor;

void main()
{
	float n = u_camera_nearfar.x;
	float f = u_camera_nearfar.y;
	float z = texture2D(u_texture,v_uv).x;
	if( n == 0.0 && f == 1.0 )
		FragColor = vec4(z);
	else
		FragColor = vec4( n * (z + 1.0) / (f + n - z * (f - n)) );
}


\instanced.vs

#version 330 core

in vec3 a_vertex;
in vec3 a_normal;
in vec2 a_coord;

in mat4 u_model;

uniform vec3 u_camera_pos;

uniform mat4 u_viewprojection;

//this will store the color for the pixel shader
out vec3 v_position;
out vec3 v_world_position;
out vec3 v_normal;
out vec2 v_uv;

void main()
{	
	//calcule the normal in camera space (the NormalMatrix is like ViewMatrix but without traslation)
	v_normal = (u_model * vec4( a_normal, 0.0) ).xyz;
	
	//calcule the vertex in object space
	v_position = a_vertex;
	v_world_position = (u_model * vec4( a_vertex, 1.0) ).xyz;
	
	//store the texture coordinates
	v_uv = a_coord;

	//calcule the position of the vertex using the matrices
	gl_Position = u_viewprojection * vec4( v_world_position, 1.0 );
}


\phong.vs

#version 330 core

in vec3 a_vertex;
in vec3 a_normal;
in vec2 a_coord;
in vec4 a_color;

uniform vec3 u_camera_position;
uniform mat4 u_model;
uniform mat4 u_viewprojection;

out vec3 v_position;
out vec3 v_world_position;
out vec3 v_normal;
out vec2 v_uv;
out vec4 v_color;
out vec3 v_camera_position;

void main()
{
    // Calculate the normal in world space
    v_normal = normalize((u_model * vec4(a_normal, 0.0)).xyz);
    
    // Calculate the vertex in object space
    v_position = a_vertex;
    v_world_position = (u_model * vec4(v_position, 1.0)).xyz;
    
    // Store the color and texture coordinates
    v_color = a_color;
    v_uv = a_coord;
    
    // Pass camera position to fragment shader
    v_camera_position = u_camera_position;

    // Calculate the position of the vertex
    gl_Position = u_viewprojection * vec4(v_world_position, 1.0);
}

\phong.fs
#version 330 core

in vec3 v_position;
in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_color;
in vec3 v_camera_position;

uniform vec4 u_color;
uniform sampler2D u_texture;
uniform float u_time;
uniform float u_alpha_cutoff;
uniform float u_shininess;

// Lighting uniforms
uniform vec3 u_ambient_light;           // Ambient light for the scene
uniform vec3 u_light_pos[10];           // Light positions
uniform vec3 u_light_color[10];         // Light colors
uniform float u_light_intensity[10];    // Light intensities
uniform int u_light_count;              // Number of active lights
uniform int u_light_type[10];
uniform vec3 u_light_dir[10];
uniform vec2 u_light_cone[10];

uniform sampler2D u_shadow_map;
uniform mat4 u_shadow_matrix;

out vec4 FragColor;

void main()
{
    vec2 uv = v_uv;
    vec4 color = u_color;
    color *= texture(u_texture, uv);

    // Shininess
    if(color.a < u_alpha_cutoff)
        discard;

    // Normalize vectors
    vec3 N = normalize(v_normal);
    vec3 V = normalize(v_camera_position - v_world_position);
    
    vec3 K = color.rgb;
    
    // Initialize lighting components
    vec3 ambient = vec3(0.0);
    vec3 diffuse = vec3(0.0);
    vec3 specular = vec3(0.0);
    vec3 final_color = vec3(0.0);

    // 3.3 ASSIGNMENT 3
    vec4 shadow_coord = u_shadow_matrix * vec4(v_world_position, 1.0);
    shadow_coord.xyz /= shadow_coord.w;

    float shadow_depth = shadow_coord.z;
    vec2 shadow_uv = shadow_coord.xy;

    // Check if fragment is outside shadow map bounds
    float shadow_factor = 1.0;
    if (shadow_uv.x >= 0.0 && shadow_uv.x <= 1.0 && shadow_uv.y >= 0.0 && shadow_uv.y <= 1.0)
    {
        float closest_depth = texture(u_shadow_map, shadow_uv).r;
        float bias = 0.005;
        if (shadow_depth - bias > closest_depth)
            shadow_factor = 0.5; // In shadow
    }
    
    // Ambient component 
    ambient = u_ambient_light;
    
    // Process each light
    for(int i = 0; i < u_light_count; i++)
    {
        if(u_light_type[i] == 1){ //Point
			// Light direction and distance
            vec3 L = normalize(u_light_pos[i] - v_world_position);
            float distance = length(u_light_pos[i] - v_world_position);
        
            // Attenuation
            float attenuation = 1.0 / (distance * distance);
            vec3 light_intensity = u_light_color[i] * u_light_intensity[i] * attenuation;
        
            // Diffuse component
            float NdotL = clamp(dot(L, N), 0.0, 1.0);
            diffuse += NdotL * light_intensity;
        
            // Specular component (Phong)
            vec3 R = reflect(L, N);
            float RdotV = clamp(dot(R, V), 0.0, 1.0);
            specular += pow(RdotV, u_shininess) * light_intensity;
            
            // Combine all components: K * (ambient + diffuse + specular)
            final_color = K * (ambient + diffuse + specular);
        
        
        } else if(u_light_type[i] == 2){ // Spot light
            vec3 light_dir = u_light_pos[i] - v_world_position;
            float distance = length(light_dir);
            vec3 L = normalize(light_dir);

            vec3 spot_dir = normalize(u_light_dir[i]);
            float theta = dot(L, spot_dir);

            float outer = cos(u_light_cone[i].y);
            float inner = cos(u_light_cone[i].x);

            if(theta > outer){
                float epsilon = inner - outer;
                float spotlight_factor = clamp((theta - outer) / epsilon, 0.0, 1.0);

                float attenuation = 1.0 / (distance * distance);

                vec3 light_intensity = u_light_color[i] * u_light_intensity[i] * attenuation * spotlight_factor;

                // Diffuse component
                float NdotL = clamp(dot(L, N), 0.0, 1.0);
                diffuse += NdotL * light_intensity;

                // Specular component (Phong)
                vec3 R = reflect(L, N);
                float RdotV = clamp(dot(R, V), 0.0, 1.0);
                specular += pow(RdotV, u_shininess) * light_intensity;
            }
        } else if (u_light_type[i] == 3){ //Directional
            vec3 L = normalize(-u_light_dir[i]);
            float distance = length(u_light_pos[i] - v_world_position);
        
            // Attenuation
            float attenuation = 1.0; //No attenuation
            vec3 light_intensity = u_light_color[i] * u_light_intensity[i] * attenuation;
        
            // Diffuse component
            float NdotL = clamp(dot(L, N), 0.0, 1.0);
            diffuse += NdotL * light_intensity;
        
            // Specular component (Phong)
            vec3 R = reflect(L, N);
            float RdotV = clamp(dot(R, V), 0.0, 1.0);
            specular += pow(RdotV, u_shininess) * light_intensity;
            
            // Combine all components: K * (ambient + diffuse + specular)
            final_color = K * (ambient + diffuse + specular);

        } else{
            final_color = K; //No light


        }
       
        
    }
    
    // Output final color with original alpha
    FragColor = vec4(final_color, color.a);
}

\phong_multipass_ambient.fs
#version 330 core

in vec3 v_position;
in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_color;
in vec3 v_camera_position;

uniform vec4 u_color;
uniform sampler2D u_texture;
uniform float u_alpha_cutoff;

uniform vec3 u_ambient_light;

out vec4 FragColor;

void main()
{
    vec4 color = u_color * texture(u_texture, v_uv);
    if(color.a < u_alpha_cutoff)
        discard;
        
    // Solo aplicamos la componente ambiental
    FragColor = vec4(color.rgb * u_ambient_light, color.a);
}

\phong_multipass_light.fs
#version 330 core

in vec3 v_position;
in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_color;
in vec3 v_camera_position;

uniform vec4 u_color;
uniform sampler2D u_texture;
uniform float u_alpha_cutoff;
uniform float u_shininess;

uniform vec3 u_light_pos;
uniform vec3 u_light_color;
uniform float u_light_intensity;
uniform int u_light_type;
uniform vec3 u_light_dir;
uniform vec2 u_light_cone;

uniform sampler2D u_shadow_map;
uniform mat4 u_shadow_matrix;

out vec4 FragColor;

void main()
{
    vec2 uv = v_uv;
    vec4 color = u_color * texture(u_texture, uv);

    if(color.a < u_alpha_cutoff)
        discard;

    vec3 N = normalize(v_normal);
    vec3 V = normalize(v_camera_position - v_world_position);
    vec3 K = color.rgb;

    vec3 diffuse = vec3(0.0);
    vec3 specular = vec3(0.0);
    vec3 final_color = vec3(0.0);

    // Shadow mapping
    vec4 shadow_coord = u_shadow_matrix * vec4(v_world_position, 1.0);
    shadow_coord.xyz /= shadow_coord.w;
    float shadow_depth = shadow_coord.z;
    vec2 shadow_uv = shadow_coord.xy;

    float shadow_factor = 1.0;
    if (shadow_uv.x >= 0.0 && shadow_uv.x <= 1.0 && shadow_uv.y >= 0.0 && shadow_uv.y <= 1.0)
    {
        float closest_depth = texture(u_shadow_map, shadow_uv).r;
        float bias = 0.005;
        if (shadow_depth - bias > closest_depth)
            shadow_factor = 0.5;
    }

    if(u_light_type == 1) { // Point light
        vec3 L = normalize(u_light_pos - v_world_position);
        float distance = length(u_light_pos - v_world_position);
        float attenuation = 1.0 / (distance * distance);
        vec3 light_intensity = u_light_color * u_light_intensity * attenuation;

        float NdotL = clamp(dot(L, N), 0.0, 1.0);
        diffuse = NdotL * light_intensity;

        vec3 R = reflect(L, N);
        float RdotV = clamp(dot(R, V), 0.0, 1.0);
        specular = pow(RdotV, u_shininess) * light_intensity;

        final_color = K * (diffuse + specular) * shadow_factor;

    } else if(u_light_type == 2){ // Spot light
        vec3 light_dir = u_light_pos - v_world_position;
        float distance = length(light_dir);
        vec3 L = normalize(light_dir);

        vec3 spot_dir = normalize(u_light_dir);
        float theta = dot(L, spot_dir);

        float outer = cos(u_light_cone.y);
        float inner = cos(u_light_cone.x);

        if(theta > outer){
            float epsilon = inner - outer;
            float spotlight_factor = clamp((theta - outer) / epsilon, 0.0, 1.0);

            float attenuation = 1.0 / (distance * distance);
            vec3 light_intensity = u_light_color * u_light_intensity * attenuation * spotlight_factor;

            float NdotL = clamp(dot(L, N), 0.0, 1.0);
            diffuse = NdotL * light_intensity;

            vec3 R = reflect(L, N);
            float RdotV = clamp(dot(R, V), 0.0, 1.0);
            specular = pow(RdotV, u_shininess) * light_intensity;

            final_color = K * (diffuse + specular) * shadow_factor;
        }
    } else if (u_light_type == 3){ // Directional light
        vec3 L = normalize(-u_light_dir);
        vec3 light_intensity = u_light_color * u_light_intensity;

        float NdotL = clamp(dot(L, N), 0.0, 1.0);
        diffuse = NdotL * light_intensity;

        vec3 R = reflect(L, N);
        float RdotV = clamp(dot(R, V), 0.0, 1.0);
        specular = pow(RdotV, u_shininess) * light_intensity;

        final_color = K * (diffuse + specular) * shadow_factor;

    } else {
        final_color = K;
    }

    FragColor = vec4(final_color, color.a);
}
