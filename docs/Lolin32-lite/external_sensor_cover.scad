$fn=180;
innerWidth=46.7;
innerHeight=38.5;
innerLength=58;
thickness=2;

module boltplace(length=10, outer=4, inner=1.8)
{
    difference() {
        cylinder(h=length, d=outer);
        cylinder(h=length, d=inner);
    }
};

union() {
    difference() {
        cube([innerWidth + 2 * thickness, innerLength+thickness-1, innerHeight + 2 * thickness]);
        translate([thickness, 0, thickness]) {
            cube([innerWidth, innerLength, innerHeight]);
            translate([innerWidth, 11.5, innerHeight-7.5]) rotate([0,90,0]) cylinder(h=10, d=2, center=true);
            translate([innerWidth, 20, innerHeight-5-2.3]) cube([10,16,5]);
            translate([innerWidth, 46.5, innerHeight-3.8-1.5]) cube([10,7.2,3.8]);
        }
    }

    translate([-1,0,2]) rotate([-90,0,0]) boltplace();
    translate([innerWidth+2*thickness+1,0,2]) rotate([-90,0,0]) boltplace();
    translate([2,0,innerHeight+5]) rotate([-90,0,0]) boltplace();
    translate([innerWidth+2*thickness-2,0,innerHeight+5]) rotate([-90,0,0]) boltplace();

}

translate([0,0,50])
union() {
    translate([-1,0,2]) rotate([-90,0,0]) boltplace(length=thickness);
    translate([innerWidth+2*thickness+1,0,2]) rotate([-90,0,0]) boltplace(length=thickness);
    translate([2,0,innerHeight+5]) rotate([-90,0,0]) boltplace(length=thickness);
    translate([innerWidth+2*thickness-2,0,innerHeight+5]) rotate([-90,0,0]) boltplace(length=thickness);
    cube([innerWidth+2*thickness, thickness, innerHeight+2*thickness]);
}
