flat basic.vs flat.fs
texture basic.vs texture.fs
skybox basic.vs skybox.fs
depth quad.vs depth.fs
multi basic.vs multi.fs
phong phong.vs phong.fs
phong_multipass_ambient phong.vs phong_multipass_ambient.fs
phong_multipass_light phong.vs phong_multipass_light.fs
plain basic.vs plain.fs
compute test.cs
gbuffer_fill basic.vs gbuffer_fill.fs
phong_deferred quad.vs deferred_single.fs
light_volume light_volume.vs light_volume.fs
deferred_ambient quad.vs deferred_ambient.fs
ssao quad.vs ssao.fs
tonemap quad.vs tonemap.fs

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
#define MAX_LIGHTS 10
#define MAX_SHADOWS 4

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

uniform vec3 u_ambient_light;

uniform vec3 u_light_pos[MAX_LIGHTS];
uniform vec3 u_light_color[MAX_LIGHTS];
uniform float u_light_intensity[MAX_LIGHTS];
uniform int u_light_type[MAX_LIGHTS];
uniform vec3 u_light_dir[MAX_LIGHTS];
uniform vec2 u_light_cone[MAX_LIGHTS];

uniform int u_light_count;
uniform int u_numShadows;

uniform float u_bias;

// Shadow maps and matrices
uniform sampler2D u_shadow_map_0;
uniform sampler2D u_shadow_map_3;
uniform mat4 u_shadow_matrix_0;
uniform mat4 u_shadow_matrix_3;

out vec4 FragColor;

// Compute shadow factor from a given map and matrix
float computeShadow(sampler2D shadow_map, mat4 shadow_matrix) {
    vec4 shadow_coord = shadow_matrix * vec4(v_world_position, 1.0);
    shadow_coord.xyz /= shadow_coord.w;
    vec2 shadow_uv = shadow_coord.xy * 0.5 + 0.5;

    // If outside shadow map, return 1.0 (no shadow)
    if (shadow_uv.x < 0.0 || shadow_uv.x > 1.0 || shadow_uv.y < 0.0 || shadow_uv.y > 1.0)
        return 1.0;

    float closest_depth = texture(shadow_map, shadow_uv).r;
    float current_depth = shadow_coord.z * 0.5 + 0.5;

    return (current_depth - u_bias > closest_depth) ? 0.0 : 1.0;
}

void main() {
    vec2 uv = v_uv;
    vec4 color = u_color * texture(u_texture, uv);

    if(color.a < u_alpha_cutoff)
        discard;

    vec3 N = normalize(v_normal);
    vec3 V = normalize(v_camera_position - v_world_position);
    vec3 K = color.rgb;

    vec3 ambient = u_ambient_light;
    vec3 diffuse = vec3(0.0);
    vec3 specular = vec3(0.0);

    for(int i = 0; i < u_light_count; i++) {
        vec3 L;
        float attenuation = 1.0;
        float spotlight_factor = 1.0;
        float shadow = 1.0;

        if(u_light_type[i] == 1) { // Point light
            vec3 light_vec = u_light_pos[i] - v_world_position;
            float distance = length(light_vec);
            L = normalize(light_vec);
            attenuation = 1.0 / (distance * distance);
            if (i == 0) shadow = computeShadow(u_shadow_map_0, u_shadow_matrix_0);
        }
        else if(u_light_type[i] == 2) { // Spot light
            vec3 light_vec = u_light_pos[i] - v_world_position;
            float distance = length(light_vec);
            L = normalize(light_vec);
            vec3 dir = normalize(u_light_dir[i]);
            float theta = dot(L, dir);
            float outer = cos(u_light_cone[i].y);
            float inner = cos(u_light_cone[i].x);
            float epsilon = inner - outer;
            spotlight_factor = clamp((theta - outer) / epsilon, 0.0, 1.0);
            attenuation = 1.0 / (distance * distance);
            if (i == 0) shadow = computeShadow(u_shadow_map_0, u_shadow_matrix_0);
        }
        else if(u_light_type[i] == 3) { // Directional light
            L = normalize(-u_light_dir[i]);
            attenuation = 1.0;
            spotlight_factor = 1.0;
            if (i == 3) shadow = computeShadow(u_shadow_map_3, u_shadow_matrix_3);
        }
        else {
            continue;
        }

        vec3 light_intensity = u_light_color[i] * u_light_intensity[i] * attenuation * spotlight_factor * shadow;

        float NdotL = max(dot(N, L), 0.0);
        vec3 R = reflect(-L, N);
        float RdotV = max(dot(R, V), 0.0);

        diffuse += NdotL * light_intensity;
    float specular_factor = pow(RdotV, u_shininess) * 16.0; 
    specular += specular_factor * light_intensity * step(0.0, NdotL);
    }

    vec3 final_color = K * (ambient + diffuse) + specular;

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

    FragColor = vec4(clamp(final_color, 0.0, 1.0), color.a);
}

\plain.fs

#version 330 core
in vec2 v_uv;

uniform int u_mask;
uniform float u_alpha_cutoff;
uniform sampler2D u_op_map;

out vec4 FragColor;

void main()
{
    if (u_mask == 1) {
        float a = texture(u_op_map, v_uv).r;
        if (a < u_alpha_cutoff) discard;
    }

	FragColor = vec4(0.0);
}


\gbuffer_fill.fs
#version 330 core

in vec3 v_normal;
in vec2 v_uv;
in vec3 v_world_position;

layout(location = 0) out vec4 gbuffer_albedo;
layout(location = 1) out vec4 gbuffer_normal;

uniform sampler2D u_color_texture;
uniform sampler2D u_metallic_roughness_texture;
uniform vec4 u_color;
uniform float u_alpha_cutoff;

void main()
{
    vec4 tex_color = texture(u_color_texture, v_uv);
    vec3 final_color = tex_color.rgb * u_color.rgb;

    // Opcional: alpha masking per a objectes amb textures transparents
    float alpha = tex_color.a * u_color.a;
    if(alpha < u_alpha_cutoff)
        discard;

    // Output cap al G-Buffer
    gbuffer_albedo = vec4(final_color, 1.0);

    // Encode normal a [0,1] per emmagatzemar-la com a textura
    vec3 encoded_normal = normalize(v_normal) * 0.5 + 0.5;
    gbuffer_normal = vec4(encoded_normal, 1.0);
}


\deferred_single.fs
#version 330 core

#define MAX_LIGHTS 10
#define MAX_SHADOWS 4

#include "PBR_functions"

in vec2 uv;

// G-Buffer textures
uniform sampler2D u_gbuffer_color;
uniform sampler2D u_gbuffer_normal;
uniform sampler2D u_gbuffer_depth;
uniform sampler2D u_ssao_map;


// Camera info
uniform mat4 u_inverse_viewprojection;
uniform vec3 u_camera_position;
uniform vec2 u_camera_nearfar;

// Lighting uniforms
uniform vec3 u_ambient_light;
uniform int u_light_count;
uniform float u_bias;

// Light arrays
uniform vec3 u_light_pos[MAX_LIGHTS];
uniform vec3 u_light_color[MAX_LIGHTS];
uniform float u_light_intensity[MAX_LIGHTS];
uniform int u_light_type[MAX_LIGHTS]; // 1=point, 2=spot, 3=directional
uniform vec3 u_light_dir[MAX_LIGHTS];
uniform vec2 u_light_cone[MAX_LIGHTS]; // x=inner angle, y=outer angle

uniform vec2 u_res_inv;

// Shadow maps
uniform sampler2D u_shadow_map_0;
uniform sampler2D u_shadow_map_3;
uniform mat4 u_shadow_matrix_0;
uniform mat4 u_shadow_matrix_3;

out vec4 FragColor;

vec3 reconstructPosition(vec2 uv, float depth) {
    float z = depth * 2.0 - 1.0;
    vec2 uv_clip = uv * 2.0 - 1.0;
    vec4 clip_coords = vec4(uv_clip.x, uv_clip.y, z, 1.0);
    vec4 world_pos = u_inverse_viewprojection * clip_coords;
    return world_pos.xyz / world_pos.w;
}

float computeShadow(sampler2D shadow_map, mat4 shadow_matrix, vec3 world_position) {
    vec4 shadow_coord = shadow_matrix * vec4(world_position, 1.0);
    shadow_coord.xyz /= shadow_coord.w;
    vec2 shadow_uv = shadow_coord.xy * 0.5 + 0.5;

    // If outside shadow map, return 1.0 (no shadow)
    if (shadow_uv.x < 0.0 || shadow_uv.x > 1.0 || shadow_uv.y < 0.0 || shadow_uv.y > 1.0)
        return 1.0;

    float closest_depth = texture(shadow_map, shadow_uv).r;
    float current_depth = shadow_coord.z * 0.5 + 0.5;

    return (current_depth > closest_depth) ? 0.0 : 1.0;
}

void main()
{

    vec2 uv = gl_FragCoord.xy * u_res_inv;
    float occlusion = texture(u_ssao_map, uv).r;

    vec4 albedo_spec = texture(u_gbuffer_color, uv);
    vec3 albedo = albedo_spec.rgb;
    float roughness = albedo_spec.a;

    vec4 normal_metal = texture(u_gbuffer_normal, uv);
    vec3 N = normalize(normal_metal.rgb * 2.0 - 1.0);
    float metalness = normal_metal.a;

    vec3 F0 = mix(vec3(0.04), albedo, metalness);

    // Read G-Buffer data
    vec3 K = albedo_spec.rgb;
    float shininess = albedo_spec.a * 64.0; // Adjust shininess factor
    
    float depth = texture(u_gbuffer_depth, uv).r;

    if (depth >= 1.0) {
        discard;
    }

    vec3 world_position = reconstructPosition(uv, depth);
    
    vec3 V = normalize(u_camera_position - world_position);
    
    vec3 final_color = K * u_ambient_light;
    
    for(int i = 0; i < u_light_count && i < MAX_LIGHTS; i++) {
        vec3 L;
        float attenuation = 1.0;
        float spotlight_factor = 1.0;
        float shadow = 1.0;

        if(u_light_type[i] == 1) { // Point light
            vec3 light_vec = u_light_pos[i] - world_position;
            float distance = length(light_vec);
            L = normalize(light_vec);
            attenuation = 1.0 / (distance * distance);
            
            //if(i == 0) shadow = computeShadow(u_shadow_map_0, u_shadow_matrix_0, world_position);
        }
        else if(u_light_type[i] == 2) { // Spot light
            vec3 light_vec = u_light_pos[i] - world_position;
            float distance = length(light_vec);
            L = normalize(light_vec);
            vec3 dir = normalize(u_light_dir[i]);
            float theta = dot(L, dir);
            float outer = cos(u_light_cone[i].y);
            float inner = cos(u_light_cone[i].x);
            float epsilon = inner - outer;
            spotlight_factor = clamp((theta - outer) / epsilon, 0.0, 1.0);
            attenuation = 1.0 / (distance * distance);
            
            if(i == 0) shadow = computeShadow(u_shadow_map_0, u_shadow_matrix_0, world_position);
        }
        else if(u_light_type[i] == 3) { // Directional light
            L = normalize(u_light_dir[i]);
            
            if(i == 3) shadow = computeShadow(u_shadow_map_3, u_shadow_matrix_3, world_position);
        }
        else {
            continue;
        }

        vec3 light_intensity = u_light_color[i] * u_light_intensity[i] * attenuation * spotlight_factor * shadow;

        vec3 H = normalize(L + V);
        float NdotL = max(dot(N, L), 0.0);
        float NdotV = max(dot(N, V), 0.0);

        float NDF = distributionGGX(N, H, roughness);
        float G   = geometrySmith(N, V, L, roughness);
        vec3  F   = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * NdotV * NdotL + 0.001;
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = (1.0 - kS) * (1.0 - metalness);

        light_intensity = u_light_color[i] * u_light_intensity[i] * attenuation * spotlight_factor * shadow;
        vec3 diffuse = (kD * albedo / 3.141592) * NdotL;

        final_color += (diffuse + specular) * light_intensity;

    }
    vec3 kS = fresnelSchlick(max(dot(N, V), 0.0), F0);
    vec3 kD = (1.0 - kS) * (1.0 - metalness);
    vec3 ambient = u_ambient_light * (albedo * kD) * occlusion;

    FragColor = vec4(final_color + ambient, 1.0);
}

\light_volume.vs
#version 330 core

in vec3 a_vertex;       // Posición del vértice (esfera unitaria)
in vec3 a_normal;       // Normal (no siempre necesaria para light volumes)

uniform mat4 u_model;           // Transformación de la luz (posición + escala)
uniform mat4 u_viewprojection;  // View + Projection
uniform vec3 u_camera_pos;      // Posición de cámara (para efectos opcionales)

out vec3 v_world_position;      // Posición en mundo del vértice
out vec3 v_normal;              // Normal (opcional, para efectos avanzados)

void main()
{
    // Transformar vértice a mundo (la esfera se escala según el radio de influencia de la luz)
    v_world_position = (u_model * vec4(a_vertex, 1.0)).xyz;
    
    // Pasar la normal (útil si quieres hacer efectos como "rim lighting" en el volumen)
    v_normal = normalize((u_model * vec4(a_normal, 0.0)).xyz);
    
    // Posición en clip space
    gl_Position = u_viewprojection * vec4(v_world_position, 1.0);
}


\light_volume.fs
#version 330 core

uniform vec3 u_camera_position;
uniform mat4 u_inverse_viewprojection;
uniform vec2 u_res_inv;

uniform vec3 u_light_pos;
uniform vec3 u_light_color;
uniform float u_light_intensity;
uniform int u_light_type;
uniform vec3 u_light_dir;
uniform vec2 u_light_cone;

uniform sampler2D u_gbuffer_color;
uniform sampler2D u_gbuffer_normal;
uniform sampler2D u_gbuffer_depth;

#include "PBR_functions"


out vec4 FragColor;

vec3 getPosition(vec2 uv, float depth) {
    vec4 pos = u_inverse_viewprojection * vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    return pos.xyz / pos.w;
}

void main() {
    vec2 uv = gl_FragCoord.xy * u_res_inv;

    float depth = texture(u_gbuffer_depth, uv).r;
    if (depth >= 1.0) discard;

    vec3 pos = getPosition(uv, depth);
    vec4 albedo_spec = texture(u_gbuffer_color, uv);
    vec4 normal_metal = texture(u_gbuffer_normal, uv);

    vec3 albedo = albedo_spec.rgb;
    float roughness = albedo_spec.a;
    vec3 normal = normalize(normal_metal.rgb * 2.0 - 1.0);
    float metalness = normal_metal.a;

    vec3 light_vec = u_light_pos - pos;
    float dist = length(light_vec);
    vec3 L = normalize(light_vec);
    vec3 V = normalize(u_camera_position - pos);
    vec3 H = normalize(V + L);

    float NdotL = max(dot(normal, L), 0.0);
    float NdotV = max(dot(normal, V), 0.0);

    // Attenuation
    float att = u_light_intensity / (dist * dist);

    // Spotlight
    if (u_light_type == 2) {
        float cos_angle = dot(-L, normalize(u_light_dir));
        float spot = smoothstep(u_light_cone.y, u_light_cone.x, cos_angle);
        att *= spot;
    }

    // PBR: Cook-Torrance
    vec3 F0 = mix(vec3(0.04), albedo, metalness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    float D = distributionGGX(normal, H, roughness);
    float G = geometrySmith(normal, V, L, roughness);

    vec3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 0.001);
    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - metalness);
    vec3 diffuse = kD * albedo / 3.141592;

    vec3 radiance = u_light_color * att;
    vec3 color = (diffuse + specular) * radiance * NdotL;

    FragColor = vec4(vec3(color), 1.0);
}


\deferred_ambient.fs
#version 330 core

in vec2 uv;

// G-Buffer textures
uniform sampler2D u_gbuffer_color;
uniform sampler2D u_gbuffer_normal;
uniform sampler2D u_gbuffer_depth;

// Camera info
uniform mat4 u_inverse_viewprojection;
uniform vec3 u_camera_position;

// Ambient light
uniform vec3 u_ambient_light;

uniform vec2 u_res_inv;

out vec4 FragColor;

#include "PBR_functions"


vec3 reconstructPosition(vec2 uv, float depth) {
    float z = depth * 2.0 - 1.0;
    vec2 uv_clip = uv * 2.0 - 1.0;
    vec4 clip_coords = vec4(uv_clip.x, uv_clip.y, z, 1.0);
    vec4 world_pos = u_inverse_viewprojection * clip_coords;
    return world_pos.xyz / world_pos.w;
}

void main() {
    vec2 uv = gl_FragCoord.xy * u_res_inv;

    vec4 albedo_spec = texture(u_gbuffer_color, uv);
    vec3 albedo = albedo_spec.rgb;
    float roughness = albedo_spec.a;

    vec4 normal_metal = texture(u_gbuffer_normal, uv);
    float metalness = normal_metal.a;

    float depth = texture(u_gbuffer_depth, uv).r;
    if (depth >= 1.0) discard;

    vec3 F0 = mix(vec3(0.04), albedo, metalness);
    vec3 kS = fresnelSchlick(1.0, F0); // full reflection
    vec3 kD = (1.0 - kS) * (1.0 - metalness);

    vec3 ambient = u_ambient_light * albedo * kD;

    FragColor = vec4(ambient, 1.0);
}


\PBR_functions

// PBR_functions.glsl

// Fresnel - Schlick approximation
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// Normal Distribution Function (GGX)
float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0001);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = 3.141592 * denom * denom;

    return a2 / max(denom, 0.001); // prevent divide by zero
}

// Geometry function: Smith's method with Schlick-GGX
float geometrySchlickGGX(float NdotV, float roughness) {
    float a = roughness * roughness;
    float k = a/2.0;

    return NdotV / max(NdotV * (1.0 - k) + k, 0.001);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = geometrySchlickGGX(NdotV, roughness);
    float ggx2 = geometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

\ssao.fs
#version 330 core

uniform sampler2D u_gbuffer_depth;
uniform sampler2D u_gbuffer_normal;
uniform vec3 u_sample_pos[64];
uniform int u_sample_count;
uniform float u_sample_radius;
uniform mat4 u_p_mat;
uniform mat4 u_inv_p_mat;
uniform vec2 u_res_inv;
uniform int u_use_ssao_plus;

in vec2 v_uv;
out vec4 FragColor;

vec3 viewPosFromDepth(float depth, vec2 texCoord) {
    vec4 clipPos = vec4(texCoord * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = u_inv_p_mat * clipPos;
    return viewPos.xyz / viewPos.w;
}

void main()
{
    // Get depth and normal from GBuffer
    float depth = texture(u_gbuffer_depth, v_uv).r;
    vec3 normal = normalize(texture(u_gbuffer_normal, v_uv).xyz * 2.0 - 1.0);
    
    // Background (depth = 1.0) - no occlusion
    if (depth >= 1.0) {
        FragColor = vec4(1.0);
        return;
    }
    
    vec3 fragPos = viewPosFromDepth(depth, v_uv);
    
    // Generate random rotation using noise
    vec3 randomVec = normalize(vec3(
        fract(sin(dot(v_uv, vec2(12.9898, 78.233))) * 43758.5453),
        fract(sin(dot(v_uv, vec2(39.346, 11.135))) * 43758.5453),
        0.0
    ));
    
    // Create TBN matrix to orient samples
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);
    
    // Calculate occlusion
    float occlusion = 0.0;
    for(int i = 0; i < u_sample_count; i++) {
        // Get sample position in view space
        vec3 samplePos = TBN * u_sample_pos[i]; // From tangent to view space
        samplePos = fragPos + samplePos * u_sample_radius;
        
        // Project sample position to screen space
        vec4 offset = vec4(samplePos, 1.0);
        offset = u_p_mat * offset;
        offset.xyz /= offset.w;
        offset.xy = offset.xy * 0.5 + 0.5; // Transform to [0,1] range
        
        // Check if sample is outside screen
        if(offset.x < 0.0 || offset.x > 1.0 || offset.y < 0.0 || offset.y > 1.0) {
            continue;
        }
        
        // Get sample depth from GBuffer
        float sampleDepth = texture(u_gbuffer_depth, offset.xy).r;
        vec3 sampleViewPos = viewPosFromDepth(sampleDepth, offset.xy);
        
        float rangeCheck = smoothstep(0.0, 1.0, u_sample_radius / abs(fragPos.z - sampleViewPos.z));
        
        if (u_use_ssao_plus == 1) {
            // SSAO+: Only count samples in front of the surface
            occlusion += (sampleViewPos.z >= samplePos.z ? 1.0 : 0.0) * rangeCheck;
        } else {
            // Regular SSAO: Compare view-space Z values
            occlusion += (sampleViewPos.z >= samplePos.z ? 1.0 : 0.0) * rangeCheck;
        }
    }
    
    // Normalize and invert occlusion
    occlusion = 1.0 - (occlusion / float(u_sample_count));
    
    occlusion = pow(occlusion, 2.0);
    
    FragColor = vec4(occlusion, occlusion, occlusion, 1.0);
}

\tonemap.fs
#version 330 core

in vec2 v_uv;
out vec4 FragColor;

uniform sampler2D u_hdr_texture;
uniform float u_exposure;
uniform bool u_apply_gamma;
uniform int u_tone_operator;

vec3 ACESFilm(vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
    vec3 hdr = texture(u_hdr_texture, v_uv).rgb;
    vec3 mapped;

    if (u_tone_operator == 0) {
        // Linear tone mapping (identity)
        mapped = hdr * u_exposure;
    }
    else if (u_tone_operator == 1) {
        // Reinhard + exposure
        mapped = vec3(1.0) - exp(-hdr * u_exposure);
        mapped = mapped / (mapped + vec3(1.0));
    }
    else {
        // ACES Filmic
        mapped = ACESFilm(hdr * u_exposure);
    }

    if (u_apply_gamma)
        mapped = pow(mapped, vec3(1.0 / 2.2));

    FragColor = vec4(mapped, 1.0);
}