$fn=180;
module batteryCase(size, radius)
{
height=size[2];//15;
width=size[1];//46;
length=size[0];//58;
//radius = 6;
x = width-radius;
y = height*2-radius;
z = length;
hole_diameter = 1.9;
difference() {
linear_extrude(height=z)
hull()
{
    // place 4 circles in the corners, with the given radius
    translate([(-x/2)+(radius/2), (-y/2)+(radius/2), 0])
    circle(r=radius);

    translate([(x/2)-(radius/2), (-y/2)+(radius/2), 0])
    circle(r=radius);

    translate([(-x/2)+(radius/2), (y/2)-(radius/2), 0])
    circle(r=radius);

    translate([(x/2)-(radius/2), (y/2)-(radius/2), 0])
    circle(r=radius);
}

translate([-50,0,-10])
cube([100,100,100]);
}
translate([15.3,height/2-5,length/2])rotate([90,0,0])cylinder(h=6,d=hole_diameter, center = true);
translate([-15.3,height/2-5,length/2])rotate([90,0,0])cylinder(h=6,d=hole_diameter, center = true);
}

module lolin32Lite()
{
height=7;
width=25.5;
length=50;
radius = 2;
x = length-radius;
y = width-radius;
z = height;
hole_diameter=1.9;
linear_extrude(height=z)
hull()
{
    // place 4 circles in the corners, with the given radius
    translate([(-x/2)+(radius/2), (-y/2)+(radius/2), 0])
    circle(r=radius);

    translate([(x/2)-(radius/2), (-y/2)+(radius/2), 0])
    circle(r=radius);

    translate([(-x/2)+(radius/2), (y/2)-(radius/2), 0])
    circle(r=radius);

    translate([(x/2)-(radius/2), (y/2)-(radius/2), 0])
    circle(r=radius);
}
translate([-(x/2-1), (y/2 - 1.75),height]) cylinder(d=hole_diameter, h=5, center = false);
translate([(x/2-8.5), -(y/2 -1),height]) cylinder(d=hole_diameter, h=5, center = false);
}

module bme280i2c()
{
    hole_diameter=1.9;
    cube([10.5,13.5,4]);
    translate([8, 11, 4]) cylinder(d=hole_diameter, h=5, center = false);
    translate([(10.5-8)/2, 1, 4])cube([8,2,6]);
}


module sps30()
{
    cube([41.2,41.2,12.8]);
}

module stepup5V()
{
    hole_diameter=1.9;
    union()
    {
        cube([18, 24.3, 1.5]);//3.8
        translate([2,-1,-1.5-1.4+0.6]) cube([14,21.7, 2.3+1.4-0.6]);
    }
}
difference() {
translate([-23,2,15]) rotate([90,0,0]) {
color("green"){
cube([46,58,2]);
translate([0,0,-19])cube([46,2,19]);

translate([2.4,0,-19])cube([6.8,15,19]);
translate([38.7,0,-19])cube([6.8,15,19]);

translate([4.5,38,-12])cube([4.5,5,12]);

translate([13,5.7,-1])    cube([20,24.3,1]);
translate([13+0.4,27,-2.8])    cube([1.2,3,2.5]);
translate([13+18.3,27,-2.8])    cube([1.2,3,2.5]);

translate([-9+23,-13.6,-2]) rotate([0,0,0]) union()
{
    //stepup5V();
    translate([(18-1)-1.2, 24.3-1-1.5, -1]) cylinder(d=1.9, h=3, center = false);
    translate([(0+1)+1.2, 24.3-1-1.5, -1]) cylinder(d=1.9, h=3, center = false);
}

translate([35.5,40,-19])
difference() {
union()
{
    cube([5,8.5,19]);
    translate([5,6.5,2.5])rotate([0,90,0])cylinder(d=3.6,h=3, center= true);
}
translate([-1,4,5])cube([7,1,6]);
translate([-1,2,5])cube([7,2,0.2]);
translate([2.5,6.5,6.5])rotate([90,0,0])cylinder(d=1.9,h=5, center= true);
}
translate([23+15.3,29,-1.75])cylinder(h=3.5,d=3.5, center = true);
translate([23-15.3,29,-1.75])cylinder(h=3.5,d=3.5, center = true);
translate([23-11,29-26.5+2,-1.75])cylinder(h=3.5,d=3.5, center = true);
translate([23+12,29-26.5+2,-1.75])cylinder(h=3.5,d=3, center = true);
translate([23+9.75,29+12+2,-1.75])cylinder(h=3.5,d=3.5, center = true);
translate([23-12,29+20+2,-1.75])cylinder(h=3.5,d=3, center = true);
}
}
translate([-21.75+8,1,71.75-1])rotate([90,0,0])cylinder(d=2.2, h=4, center=true);
translate([-1,12.5,40.5+2]) rotate([90,-90,0]) lolin32Lite();

translate([23,10.5,72.5])rotate([-90,0,90]) bme280i2c();

translate([0,0,15])batteryCase([58, 46, 15], 6);
translate([-18.5,21.5,17])rotate([90,0,0])sps30();

translate([-9,3,45]) rotate([-90,0,0]) difference()
{
    stepup5V();
            translate([(18-1)-1.2, 24.3-1-1.5, -2]) cylinder(d=1.9, h=4, center = false);
        translate([(0+1)+1.2, 24.3-1-1.5, -2]) cylinder(d=1.9, h=4, center = false);
}
}