$fn=180;
LiAccHeight=6.5;
LiAccWidth=40;
LiAccLength=60;
module LiIon()
{
    cube([LiAccWidth, LiAccLength, LiAccHeight]);
}

DFRobotC3Width=20.5;
DFRobotC3Length=25.0;
DFRobotC3Shift=1.5;
module DFRobotC3()
{
    height=1;
    width=DFRobotC3Width;
    length=DFRobotC3Length;
    unitWidth=13.2;
    unitLength=16.6;
    unitHeight=2.4;
    usbPlugLength=7.3;
    usbPlugWidth=9;
    cube([length, width, height]);
    translate([-DFRobotC3Shift,(width-usbPlugWidth)/2,height]) cube([usbPlugLength,usbPlugWidth,3]);
    translate([length - unitLength, (width - unitWidth)/2, height]) cube([unitLength, unitWidth, unitHeight]);
}

bmeModuleHeight=11;
module bme280spi()
{
    hole_diameter=2;
    width=15;
    height=bmeModuleHeight;
    cube([1.5, width, height]);
    translate([0, width/2, 3.6]) rotate([0, 90, 0]) cylinder(d=hole_diameter, h=5, center = false);
}

module mainUnit()
{
    boardWidth=24.4;
    boardLength=34.4;
    boardHeight=1.4;
    secondaryBoardsHeight=1.0;
    spacerHeight=2.4;
    switchLength=15.3;
    switchWidth=15.3;
    converterLength=8;
    converterWidth=13.2;
    holeShift=2.2;
    translate([0,0,spacerHeight+secondaryBoardsHeight])
    {
        cube([boardLength, boardWidth, boardHeight]);
        translate([DFRobotC3Shift, 0, boardHeight + spacerHeight]) DFRobotC3();
        translate([boardLength + 1, 2.5, -3.5]) bme280spi();
    }
    translate([4,0,0]) cube([switchLength, switchWidth, secondaryBoardsHeight]);
    translate([4 + switchLength + 2.5,0,0]) cube([converterLength, converterWidth, secondaryBoardsHeight]);
    translate([holeShift, boardWidth-holeShift, -boardHeight/2])cylinder(d=1.9, h=4.4);
    translate([33.6, 1.3, -boardHeight/2])cylinder(d=1.9, h=4.4);
}

sps30Width=41;
sps30Depth=40.8;
sps30Height=12.9;
module sps30()
{
    cube([sps30Depth,sps30Width,sps30Height]);
    bigInletWidth=6.6;
    bigInletHeight=3.8;
    translate([sps30Depth, sps30Width - bigInletWidth - 2.2, 8.2])cube([4,bigInletWidth, bigInletHeight]); // big inlet
    smallInletWidth=10;
    smallInletHeight=2;
    translate([sps30Depth, sps30Width - smallInletWidth - 5.2, -0.2])cube([4,smallInletWidth, smallInletHeight]); // small inlet
    outletWidth=14.8;
    outletHeight=5;
    translate([sps30Depth, 5.5, 5.8])cube([4,outletWidth, outletHeight]); //outlet
}

sps30CableSpace=2;
sps30CaseHeight=sps30Height+0.6;
sps30CaseWalls=2;
sps30Shift=0.6;
module sps30Case()
{
    color("black") translate([0,-sps30CaseWalls,0]) union(){
    translate([0,sps30Shift,0]) cube([sps30Depth, sps30CaseWalls, sps30CaseHeight]);
    translate([-sps30CableSpace,sps30Width+sps30CaseWalls-sps30Shift,0]) cube([sps30Depth+sps30CableSpace, sps30CaseWalls, sps30CaseHeight]);
    }
}

chargePortWidth=5;
frameBaseControlWidth=25.4;
frameBaseThickness=2;
frameBaseWidth=sps30Depth+sps30CableSpace;
frameBaseLength=frameBaseControlWidth + chargePortWidth + sps30Width + 2 * (sps30CaseWalls - sps30Shift);
baseTranslation=- frameBaseLength + sps30Width+ sps30CaseWalls - sps30Shift;
chargePortStallShift=10.3;
chargePortStallHeight=14;
chargePortHolesShift=2.5;
chargePortStallWidth=5;
sideAddition=6;
module mainFrame()
{
    difference()
    {
        sps30Case();
        sps30();
    }
    translate([0, baseTranslation, 0])
    {
        difference()
        {
           union()
           {
               translate([-sps30CableSpace, -sideAddition, - frameBaseThickness])
                    cube([frameBaseWidth, frameBaseLength+2*sideAddition, frameBaseThickness]);

               translate([3.5, chargePortWidth, 0])
               {
                   translate([2.2,22.6,0]) cylinder(d=3.4, h=4.4);
                   translate([33.6,1.8,0]) cylinder(d=3.4, h=4.4);
                   translate([1.4,1.3,0]) cylinder(d=2.6, h=4.4);
                   translate([33.2,23.6,0]) cylinder(d=2.6, h=4.4);
               }
               translate([frameBaseWidth-sps30CableSpace - chargePortStallShift - chargePortStallWidth/2,2,0]) cube([chargePortStallWidth,2.5,chargePortStallHeight]);
                translate([1, -3, -3]) cylinder(d=2.8, h=24);
                translate([frameBaseWidth - 5, -3, -3]) cylinder(d=2.8, h=24);
                translate([1, frameBaseLength + 3, -3]) cylinder(d=2.8, h=24);
                translate([frameBaseWidth - 5, frameBaseLength + 3, -3]) cylinder(d=2.8, h=24);
            }
            translate([3.5, 0.5 + chargePortWidth, 1]) mainUnit();
            translate([frameBaseWidth-sps30CableSpace-chargePortStallShift,6,chargePortHolesShift]) rotate([90,0,0]) cylinder(d=2.8, h=5);
            translate([frameBaseWidth-sps30CableSpace-chargePortStallShift,6,chargePortStallHeight - chargePortHolesShift]) rotate([90,0,0]) cylinder(d=2.8, h=5);

        }
    }
}

internalShellHeight=sps30CaseHeight + LiAccHeight + frameBaseThickness;
{
    difference()
    {
        translate([-2, -2, 0]) union() {
            cube([frameBaseWidth+4,frameBaseLength+4,internalShellHeight+4]);
            translate([2,-4,2])rotate([-10,0,0])cube([7,4,internalShellHeight]);
            translate([frameBaseWidth-5,-4,2])rotate([-10,0,0])cube([7,4,internalShellHeight]);
            translate([2,frameBaseLength+4,0])rotate([10,0,0])cube([7,4,internalShellHeight]);
            translate([frameBaseWidth-5,frameBaseLength+4,0])rotate([10,0,0])cube([7,4,internalShellHeight]);
        }
        
        translate([0,0,0])cube([frameBaseWidth,frameBaseLength,internalShellHeight]);

        translate([2, 0, 2])
        {
            translate([0, -baseTranslation,0]) 
            {
                sps30();
                mainFrame();
            }
            translate([3.5, 0.5 + chargePortWidth, 1]) mainUnit();
            translate([frameBaseWidth-sps30CableSpace - chargePortStallShift - chargePortStallWidth/2 +11,2,3]) cube([chargePortStallWidth,3,8.6]);
        }

    }
}

