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
light_volume basic.vs light_volume.fs
phong_singlepass basic.vs phong_singlepass.fs

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
// Uniforms
uniform sampler2D u_texture; // Albedo
uniform sampler2D u_normal_texture;
uniform sampler2D u_metallic_roughness;

in vec2 v_uv;

layout(location = 0) out vec4 gbuffer1;
layout(location = 1) out vec4 gbuffer2;

void main() {
    vec3 albedo = texture(u_texture, v_uv).rgb;
    vec3 normal = texture(u_normal_texture, v_uv).rgb;
    vec3 mer = texture(u_metallic_roughness, v_uv).rgb;

    float roughness = mer.g;
    float metalness = mer.b;

    gbuffer1 = vec4(albedo, roughness);
    gbuffer2 = vec4(normal, metalness);
}

\deferred_single.fs
#version 330 core

#define MAX_LIGHTS 10
#define MAX_SHADOWS 4

in vec2 uv;

// G-Buffer textures
uniform sampler2D u_gbuffer_color;
uniform sampler2D u_gbuffer_normal;
uniform sampler2D u_gbuffer_depth;

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

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = 3.14159265 * denom * denom;

    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;

    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

vec3 cookTorranceBRDF(vec3 N, vec3 V, vec3 L, vec3 albedo, float roughness, float metalness)
{
    vec3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    vec3 F0 = mix(vec3(0.04), albedo, metalness);
    vec3 F = fresnelSchlick(VdotH, F0);
    float D = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);

    vec3 numerator = D * F * G;
    float denominator = max(4.0 * NdotV * NdotL, 0.001);
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metalness;

    vec3 diffuse = albedo / 3.14159265;

    return (kD * diffuse + specular) * NdotL;
}

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

    return (current_depth - u_bias > closest_depth) ? 0.0 : 1.0;
}

void main()
{
    vec2 uv = gl_FragCoord.xy * u_res_inv;

    // Read G-Buffer data
    vec4 albedo_spec = texture(u_gbuffer_color, uv);
    vec3 albedo = albedo_spec.rgb;
    float roughness = albedo_spec.a;

    vec4 normal_metal = texture(u_gbuffer_normal, uv);
    vec3 N = normalize(normal_metal.rgb * 2.0 - 1.0);
    float metalness = normal_metal.a;

    float depth = texture(u_gbuffer_depth, uv).r;
    if (depth >= 1.0)
        discard;

    vec3 world_position = reconstructPosition(uv, depth);
    vec3 V = normalize(u_camera_position - world_position);

    vec3 final_color = albedo * u_ambient_light;

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
            if(i == 0) shadow = computeShadow(u_shadow_map_0, u_shadow_matrix_0, world_position);
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
            L = normalize(-u_light_dir[i]);
            if(i == 3) shadow = computeShadow(u_shadow_map_3, u_shadow_matrix_3, world_position);
        }
        else {
            continue;
        }

        vec3 light_intensity = u_light_color[i] * u_light_intensity[i] * attenuation * spotlight_factor * shadow;

        vec3 brdf = cookTorranceBRDF(N, V, L, albedo, roughness, metalness);
        final_color += brdf * light_intensity;
    }

    FragColor = vec4(final_color, 1.0);
}


\light_volume.fs
// Fragment Shader
varying vec3 v_world_position;

uniform sampler2D u_gbuffer_albedo;
uniform sampler2D u_gbuffer_normals;
uniform sampler2D u_gbuffer_depth;

uniform mat4 u_inverse_viewprojection;
uniform vec2 u_iResolution;
uniform vec3 u_camera_position;

uniform vec3 u_light_pos;
uniform vec3 u_light_color;
uniform int u_light_type;
uniform vec3 u_light_dir;
uniform vec2 u_light_cone;

vec3 getWorldPosition(vec2 uv, float depth)
{
    vec4 pos = u_inverse_viewprojection * vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    return pos.xyz / pos.w;
}

void main()
{
    vec2 uv = gl_FragCoord.xy * u_iResolution;
    float depth = texture2D(u_gbuffer_depth, uv).x;
    
    // Early exit if no geometry
    if (depth == 1.0) discard;
    
    vec3 world_pos = getWorldPosition(uv, depth);
    vec3 albedo = texture2D(u_gbuffer_albedo, uv).rgb;
    vec3 normal = texture2D(u_gbuffer_normals, uv).xyz * 2.0 - 1.0;
    
    // Vector luz -> superficie
    vec3 L = u_light_pos - world_pos;
    float dist = length(L);
    L = normalize(L);
    
    // Atenuación
    float att = 1.0 / (1.0 + dist * dist);
    
    // Spot light factor
    if (u_light_type == 2) // SPOT
    {
        float cos_angle = dot(-L, u_light_dir);
        float spot = smoothstep(u_light_cone.y, u_light_cone.x, cos_angle);
        att *= spot;
    }
    
    // Diffuse
    float NdotL = max(0.0, dot(normal, L));
    vec3 diffuse = albedo * NdotL * att * u_light_color;
    
    gl_FragColor = vec4(diffuse, 1.0);
}

\phong_singlepass.fs
#version 330 core

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = 3.14159265 * denom * denom;

    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;

    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

vec3 cookTorranceBRDF(vec3 N, vec3 V, vec3 L, vec3 albedo, float roughness, float metalness)
{
    vec3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    vec3 F0 = mix(vec3(0.04), albedo, metalness);
    vec3 F = fresnelSchlick(VdotH, F0);
    float D = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);

    vec3 numerator = D * F * G;
    float denominator = max(4.0 * NdotV * NdotL, 0.001);
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metalness;

    vec3 diffuse = albedo / 3.14159265;

    return (kD * diffuse + specular) * NdotL;
}

in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;

uniform vec3 u_camera_position;
uniform vec3 u_ambient_light;

uniform sampler2D u_texture;               // Albedo map
uniform sampler2D u_metallic_roughness;    // MER map (R: AO, G: Roughness, B: Metalness)

uniform int u_light_count;
uniform vec3 u_light_pos[10];
uniform vec3 u_light_color[10];
uniform float u_light_intensity[10];

out vec4 FragColor;

void main()
{
    vec3 albedo = texture(u_texture, v_uv).rgb;
    vec3 mer = texture(u_metallic_roughness, v_uv).rgb;

    float roughness = mer.g;
    float metalness = mer.b;

    vec3 N = normalize(v_normal);
    vec3 V = normalize(u_camera_position - v_world_position);

    vec3 final_color = albedo * u_ambient_light;

    for (int i = 0; i < u_light_count; ++i)
    {
        vec3 L = normalize(u_light_pos[i] - v_world_position);
        vec3 radiance = u_light_color[i] * u_light_intensity[i];

        vec3 brdf = cookTorranceBRDF(N, V, L, albedo, roughness, metalness);
        final_color += brdf * radiance;
    }

    FragColor = vec4(final_color, 1.0);
}