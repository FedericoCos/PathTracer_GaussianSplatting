#version 450

// --- OIT PPLL BUFFERS (Bound at Set 0) ---
// (Same structs as write shader, but bound at Set 0)

struct FragmentNode {
    vec4 color;
    uint depth;
    uint next;
};

layout(set = 0, binding = 0, std430) buffer AtomicCounter {
    uint fragmentCount; // Read-only
} atomicCounter;

layout(set = 0, binding = 1, std430) buffer FragmentList {
    FragmentNode fragments[]; // Read-only
} fragmentList;

layout(set = 0, binding = 2, r32ui) uniform uimage2DMS startOffsetImage; // Read-only


// --- INPUT (from oit_composite_vert.spv) ---
layout(location = 0) in vec2 inTexCoord;

// --- OUTPUT ---
layout(location = 0) out vec4 outColor;

// --- SETTINGS ---
const int MAX_FRAGMENTS_PER_PIXEL = 32;
const uint LIST_END = 0xFFFFFFFF;

// Local struct for sorting
struct SortNode {
    uint depth;
    vec4 color;
};

// --- Main Function ---
void main() {
    
    // Array to hold the fragments for sorting
    SortNode fragment_nodes[MAX_FRAGMENTS_PER_PIXEL];

    int count = 0;

    // --- 1. Traverse the Linked List ---
    ivec2 pixel_coord = ivec2(gl_FragCoord.xy);
    uint current_index = imageLoad(startOffsetImage, pixel_coord, gl_SampleID).r;

    while (current_index != LIST_END && count < MAX_FRAGMENTS_PER_PIXEL) {
        FragmentNode node = fragmentList.fragments[current_index];
        fragment_nodes[count].depth = node.depth;
        fragment_nodes[count].color = node.color;
        current_index = node.next;
        count++;
    }

    // --- 2. Sort the Fragments (Insertion Sort) ---
    // We sort from farthest (lowest 1/w) to nearest (highest 1/w).
    // Our depth key (1.0 / gl_FragCoord.w) is descending with distance.
    // So we sort ASCENDING (>) to get [Far, ..., Near]
    for (int i = 1; i < count; i++) {
        SortNode to_insert = fragment_nodes[i];
        int j = i;
        
        // Sort ascending (farthest first)
        while (j > 0 && fragment_nodes[j-1].depth < to_insert.depth) {
            fragment_nodes[j] = fragment_nodes[j-1];
            j--;
        }
        fragment_nodes[j] = to_insert;
    }

    // --- 3. Composite Fragments (Back-to-Front) ---
    // We are looping from i=0 (farthest) to i=count-1 (nearest)
    vec4 final_color = vec4(0.0); // Start with a fully transparent color

    for (int i = 0; i < count; i++) {
        vec3 color = fragment_nodes[i].color.rgb;
        float alpha = fragment_nodes[i].color.a;
        
        // Premultiply color
        vec3 premultiplied_color = color * alpha;

        // "Under" blending operator (Back-to-Front)
        // C_out = C_dst + C_src * (1 - A_dst)
        // A_out = A_dst + A_src * (1 - A_dst)
        
        final_color.rgb = final_color.rgb + premultiplied_color * (1.0 - final_color.a);
        final_color.a = final_color.a + alpha * (1.0 - final_color.a);

        // Optimization: stop if we're fully opaque
        if (final_color.a > 0.999) {
            break;
        }
    }
    
    // --- 4. Tonemap and Gamma Correct ---
    // The final_color.rgb is premultiplied. We must un-premultiply
    // before tonemapping for the math to be correct.
    vec3 unmultiplied_color = vec3(0.0);
    // Avoid divide by zero
    if (final_color.a > 1e-6) {
        unmultiplied_color = final_color.rgb / final_color.a;
    }

    // Now tonemap the correct linear, unmultiplied color
    vec3 tonemapped_color = unmultiplied_color / (unmultiplied_color + vec3(1.0)); // Reinhard
    tonemapped_color = pow(tonemapped_color, vec3(1.0/2.2)); // Gamma correction

    // Output the final, blended color
    // This will be blended ON TOP of the opaque scene using standard alpha blending
    // (e.g., eSrcAlpha, eOneMinusSrcAlpha), so we output NON-premultiplied color.
    outColor = vec4(tonemapped_color, final_color.a);
}

