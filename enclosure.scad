// Paramètres du boîtier
box_width = 200;
box_depth = 93;
top_height = 6;
bottom_height = 10;
wall_thickness = 3;
corner_radius = 5;          // Augmenté pour de beaux coins
edge_radius = 2;            // Nouveau: rayon des arêtes verticales

// Paramètres des composants (basés sur les données exactes)
encoder_diameter = 10;     // Trous de 1,0 cm de diamètre
button_diameter = 7;       // Trous de 0,7 cm de diamètre
mounting_hole_diameter = 4; // Trous de 0,4 cm de diamètre
led_diameter = 3;          // Estimation pour les LEDs

// Dimensions des écrans
display_width = 68;
display_height = 20;

// Positions exactes des trous (converties en mm)
encoder_positions = [
    [50, 22],   // Trou 5
    [84, 22],   // Trou 6
    [133, 22],  // Trou 7
    [167, 22],  // Trou 8
    [50, 68],   // Trou 9
    [84, 68],   // Trou 10
    [133, 68],  // Trou 11
    [167, 68]   // Trou 12
];

button_positions = [
    [14, 37],   // Trou 13 (shift)
    [14, 53]    // Trou 14 (latch)
];

mounting_holes = [
    [4, 4],     // Trou 1
    [4, 86],    // Trou 2
    [194, 4],   // Trou 3
    [194, 86]   // Trou 4
];

// Fonctions utilitaires
module rounded_cube(size, radius) {
    hull() {
        for (x = [radius, size[0] - radius]) {
            for (y = [radius, size[1] - radius]) {
                translate([x, y, 0])
                    cylinder(r = radius, h = size[2]);
            }
        }
    }
}

// Nouvelle fonction pour boîtier avec arêtes arrondies
module rounded_box(size, corner_r, edge_r) {
    hull() {
        // Coins arrondis en bas
        for (x = [corner_r, size[0] - corner_r]) {
            for (y = [corner_r, size[1] - corner_r]) {
                translate([x, y, edge_r])
                    sphere(r = edge_r);
            }
        }
        // Coins arrondis en haut
        for (x = [corner_r, size[0] - corner_r]) {
            for (y = [corner_r, size[1] - corner_r]) {
                translate([x, y, size[2] - edge_r])
                    sphere(r = edge_r);
            }
        }
    }
}

// Partie supérieure (couvercle) - 6mm avec arêtes arrondies
module top_part() {
    union() {
        difference() {
            // Boîtier principal avec arêtes arrondies
            rounded_box([box_width, box_depth, top_height], corner_radius, edge_radius);
            
            // Évidement intérieur (aussi avec arêtes arrondies)
            translate([wall_thickness, wall_thickness, wall_thickness])
                rounded_box([
                    box_width - 2 * wall_thickness, 
                    box_depth - 2 * wall_thickness, 
                    top_height
                ], corner_radius - 1, edge_radius - 0.5);
            
            // Trous pour les 8 encodeurs (positions exactes)
            for (pos = encoder_positions) {
                translate([pos[0], pos[1], -1])
                    cylinder(d = encoder_diameter, h = top_height + 2);
            }
            
            // Trous pour les 2 boutons (positions exactes)
            for (pos = button_positions) {
                translate([pos[0], pos[1], -1])
                    cylinder(d = button_diameter, h = top_height + 2);
            }
            
            // Trous pour les LEDs (1 par encodeur)
            // LEDs au-dessus des encodeurs du haut (4 premiers)
            for (i = [0:3]) {
                translate([encoder_positions[i][0], encoder_positions[i][1] - 8, -1])
                    cylinder(d = led_diameter, h = top_height + 2);
            }
            
            // LEDs en dessous des encodeurs du bas (4 derniers)
            for (i = [4:7]) {
                translate([encoder_positions[i][0], encoder_positions[i][1] + 8, -1])
                    cylinder(d = led_diameter, h = top_height + 2);
            }
            
            // LED revert mode (près du bouton latch)
            latch_button = button_positions[1];
            translate([latch_button[0], latch_button[1] + 8, -1])
                cylinder(d = led_diameter, h = top_height + 2);
            
            // Ouvertures pour les 2 écrans 68x20mm
            // Écran 1 (entre encodeurs 1-2 et 5-6)
            translate([67 - display_width/2, box_depth/2 - display_height/2, -1])
                cube([display_width, display_height, top_height + 2]);
            
            // Écran 2 (entre encodeurs 3-4 et 7-8)
            translate([150 - display_width/2, box_depth/2 - display_height/2, -1])
                cube([display_width, display_height, top_height + 2]);
            
            // Trous de montage (traversants pour vis)
            for (pos = mounting_holes) {
                translate([pos[0], pos[1], -1])
                    cylinder(d = mounting_hole_diameter, h = top_height + 2);
            }
        }
        
        // Rebords d'emboîtement (adaptés aux arêtes arrondies)
        translate([wall_thickness - 1, wall_thickness - 1, top_height - 1.5])
            difference() {
                rounded_cube([
                    box_width - 2 * (wall_thickness - 1), 
                    box_depth - 2 * (wall_thickness - 1), 
                    1.5
                ], corner_radius - 1);
                translate([1, 1, -0.1])
                    rounded_cube([
                        box_width - 2 * wall_thickness, 
                        box_depth - 2 * wall_thickness, 
                        1.7
                    ], corner_radius - 2);
            }
        
        // Texte en relief "SHIFT"
        shift_button = button_positions[0];
        translate([shift_button[0], shift_button[1] - 12, top_height - 0.5])
            linear_extrude(height = 0.8)
                text("SHIFT", size = 3, halign = "center", valign = "center", font = "Liberation Sans:style=Bold");
        
        // Texte en relief "LATCH"
        latch_button = button_positions[1];
        translate([latch_button[0], latch_button[1] - 12, top_height - 0.5])
            linear_extrude(height = 0.8)
                text("LATCH", size = 3, halign = "center", valign = "center", font = "Liberation Sans:style=Bold");
    }
}

// Partie inférieure (base) - 10mm avec arêtes arrondies
module bottom_part() {
    difference() {
        // Boîtier principal avec arêtes arrondies
        rounded_box([box_width, box_depth, bottom_height], corner_radius, edge_radius);
        
        // Évidement intérieur (aussi avec arêtes arrondies)
        translate([wall_thickness, wall_thickness, wall_thickness])
            rounded_box([
                box_width - 2 * wall_thickness, 
                box_depth - 2 * wall_thickness, 
                bottom_height
            ], corner_radius - 1, edge_radius - 0.5);
        
        // Trous pour les 8 encodeurs (traversants)
        for (pos = encoder_positions) {
            translate([pos[0], pos[1], -1])
                cylinder(d = encoder_diameter, h = bottom_height + 2);
        }
        
        // Trous pour les 2 boutons (traversants)
        for (pos = button_positions) {
            translate([pos[0], pos[1], -1])
                cylinder(d = button_diameter, h = bottom_height + 2);
        }
        
        // Trous de montage (traversants)
        for (pos = mounting_holes) {
            translate([pos[0], pos[1], -1])
                cylinder(d = mounting_hole_diameter, h = bottom_height + 2);
        }
        
        // Connecteur USB (côté gauche, près du premier trou de montage)
        translate([-1, 20, wall_thickness + 2])
            cube([wall_thickness + 2, 12, 5]);
        
        // Rainure pour l'emboîtement du couvercle (adaptée aux arêtes arrondies)
        translate([wall_thickness - 0.5, wall_thickness - 0.5, bottom_height - 2])
            difference() {
                rounded_cube([
                    box_width - 2 * (wall_thickness - 0.5), 
                    box_depth - 2 * (wall_thickness - 0.5), 
                    2.1
                ], corner_radius - 1);
                translate([0.5, 0.5, -0.1])
                    rounded_cube([
                        box_width - 2 * wall_thickness, 
                        box_depth - 2 * wall_thickness, 
                        2.3
                    ], corner_radius - 2);
            }
    }
    
    // Supports pour PCB autour des trous de montage
    pcb_support_height = 2;
    support_size = 6;
    
    for (pos = mounting_holes) {
        translate([pos[0] - support_size/2, pos[1] - support_size/2, wall_thickness])
            difference() {
                cube([support_size, support_size, pcb_support_height]);
                translate([support_size/2, support_size/2, -0.1])
                    cylinder(d = 3.2, h = pcb_support_height + 0.2);
            }
    }
}

// Génération des parties
translate([0, 0, 0]) bottom_part();
translate([box_width + 10, 0, 0]) top_part();

// Vue assemblée (décommentez pour voir l'assemblage)
// translate([0, box_depth + 20, 0]) {
//     bottom_part();
//     translate([0, 0, bottom_height - 0.1]) top_part();
// }

// Informations de debug
echo("Positions des encodeurs:");
for (i = [0:len(encoder_positions)-1]) {
    echo(str("Encodeur ", i+1, ": X=", encoder_positions[i][0], "mm, Y=", encoder_positions[i][1], "mm"));
}
echo("Positions des boutons:");
for (i = [0:len(button_positions)-1]) {
    echo(str("Bouton ", i+1, ": X=", button_positions[i][0], "mm, Y=", button_positions[i][1], "mm"));
}