#version 450

layout(early_fragment_tests) in;

// --- UNIFORMS (from vertex.spv) ---
layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
} ubo;

// --- INPUTS (from vertex_torus.spv) ---
layout(location = 0) in vec3 fragColor;

// --- OIT PPLL BUFFERS (Bound at Set 1) ---
// (This must match oit_ppll_write.frag)

struct FragmentNode {
    vec4 color; // .a is alpha
    uint depth;
    uint next;  // Index of the next node
};

layout(set = 1, binding = 0, std430) buffer AtomicCounter {
    uint fragmentCount;
} atomicCounter;

layout(set = 1, binding = 1, std430) buffer FragmentList {
    FragmentNode fragments[];
} fragmentList;

layout(set = 1, binding = 2, r32ui) uniform uimage2DMS startOffsetImage;

// --- OUTPUTS (None) ---
// We write to buffers, not to a color attachment

void main() {
    
    vec4 finalColor = vec4(fragColor, 0.5); // Hard-coded 50% alpha

    // --- Discard fully transparent fragments ---
    if (finalColor.a < 0.01) {
        discard;
    }

    // --- PPLL Insertion ---
    
    // Atomically increment the global counter
    uint index = atomicAdd(atomicCounter.fragmentCount, 1);
    
    uint max_fragments = fragmentList.fragments.length();
    if (index >= max_fragments) {
        return; // Not enough space
    }

    // Get the current "head" pointer
    ivec2 pixel_coord = ivec2(gl_FragCoord.xy);
    uint old_head = imageAtomicExchange(startOffsetImage, pixel_coord, gl_SampleID, index);

    // Write our fragment data into the list
    fragmentList.fragments[index].color = vec4(finalColor.rgb, finalColor.a); // Store linear color
    fragmentList.fragments[index].depth = floatBitsToUint(gl_FragCoord.w);
    fragmentList.fragments[index].next = old_head; // Our "next" points to the previous head
}
