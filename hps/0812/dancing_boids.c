// backup
// 1 boid to test
// cd /mnt/c/Users/sakue/desktop/boid
// gcc boid_vga.c -o boid_vga -lm
/// gcc graphics_video_16bit.c -o gr -O2 -lm

///////////////////////////////////////
/// 640x480 version! 16-bit color
/// This code will segfault the original
/// DE1 computer
/// compile with
/// gcc graphics_video_16bit.c -o gr -O2 -lm
///
///////////////////////////////////////
#include "Position_3d.h"
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
// #include "address_map_arm_brl4.h"
// boid define
#define boid_num 30
#define visual_range 20.0
#define protected_range 10.0
#define max_speed 5.0
#define min_speed 2.0
#define avoidfactor 0.05
#define matchingfactor 0.05
#define centeringfactor 0.0005
#define turnfactor 0.5

#define WIDTH 640
#define HEIGHT 480
#define cube_boundary 100
#define x_boundary cube_boundary
#define y_boundary cube_boundary
#define z_boundary cube_boundary
#define x_coordinate_display 320
#define y_coordinate_display 240
// f (focal length)是視場深度(焦距)
// d 是攝影機到投影平面的距離
// 我們假設 f = 1 和 d = 5。
double project_f = 100.0;
double project_d = 200.0;
double rx, ry, rz; // rotation of x, y, z axes
#define vertices_num 8
#define line_num 12
#define axis_num 4
// Position_3d rotate(Position_3d, double, double, double);

// video display
#define SDRAM_BASE 0xC0000000
#define SDRAM_END 0xC3FFFFFF
#define SDRAM_SPAN 0x04000000
// characters
#define FPGA_CHAR_BASE 0xC9000000
#define FPGA_CHAR_END 0xC9001FFF
#define FPGA_CHAR_SPAN 0x00002000
/* Cyclone V FPGA devices */
#define HW_REGS_BASE 0xff200000
// #define HW_REGS_SPAN        0x00200000
#define HW_REGS_SPAN 0x00005000

// graphics primitives
void VGA_text(int, int, char *);
void VGA_text_clear();
void VGA_box(int, int, int, int, short);
void VGA_rect(int, int, int, int, short);
void VGA_line(int, int, int, int, short);
void VGA_Vline(int, int, int, short);
void VGA_Hline(int, int, int, short);
void VGA_disc(int, int, int, short);
void VGA_circle(int, int, int, int);
// 16-bit primary colors
#define red (0 + (0 << 5) + (31 << 11))
#define dark_red (0 + (0 << 5) + (15 << 11))
#define green (0 + (63 << 5) + (0 << 11))
#define dark_green (0 + (31 << 5) + (0 << 11))
#define blue (31 + (0 << 5) + (0 << 11))
#define dark_blue (15 + (0 << 5) + (0 << 11))
#define yellow (0 + (63 << 5) + (31 << 11))
#define cyan (31 + (63 << 5) + (0 << 11))
#define magenta (31 + (0 << 5) + (31 << 11))
#define black (0x0000)
#define gray (15 + (31 << 5) + (51 << 11))
#define white (0xffff)
int colors[] = {red,    dark_red, green,   dark_green, blue,  dark_blue,
                yellow, cyan,     magenta, gray,       white, black};

// pixel macro
#define VGA_PIXEL(x, y, color)                                                 \
  do {                                                                         \
    int *pixel_ptr;                                                            \
    pixel_ptr = (int *)((char *)vga_pixel_ptr + (((y)*640 + (x)) << 1));       \
    *(short *)pixel_ptr = (color);                                             \
  } while (0)

// the light weight buss base
void *h2p_lw_virtual_base;

// pixel buffer
volatile unsigned int *vga_pixel_ptr = NULL;
void *vga_pixel_virtual_base;

// character buffer
volatile unsigned int *vga_char_ptr = NULL;
void *vga_char_virtual_base;

// /dev/mem file id
int fd;

// measure time
struct timeval t1, t2;
double elapsedTime;

// *************************************************************************
// O,X,Y,Z
Position_3d axis_line[4] = {{0, 0, 0}, {100, 0, 0}, {0, 100, 0}, {0, 0, 100}};

// coordinate
Position_3d cube_vertices[8] = {{-x_boundary, -y_boundary, -z_boundary},
                                {x_boundary, -y_boundary, -z_boundary},
                                {x_boundary, y_boundary, -z_boundary},
                                {-x_boundary, y_boundary, -z_boundary},
                                {-x_boundary, -y_boundary, z_boundary},
                                {x_boundary, -y_boundary, z_boundary},
                                {x_boundary, y_boundary, z_boundary},
                                {-x_boundary, y_boundary, z_boundary}};
// edge:(0,1):connect
int cube_edge[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                        {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};

// 初始化boid的位置及速度
// born in boundary
void initialize_boid(Boid boids[]) {
  int i;
  for (i = 0; i < boid_num; i++) {
    // boids[i].color = colors[rand() % 10];
    boids[i].color = colors[0];
    boids[i].r = 0.0;
    boids[i].x =
        rand() % (x_boundary / 2) * (rand() % 2 == 0 ? 1 : -1); // X: -50 ~ 50
    boids[i].y =
        rand() % (y_boundary / 2) * (rand() % 2 == 0 ? 1 : -1); // Y: -50 ~ 50
    boids[i].z =
        rand() % (z_boundary / 2) * (rand() % 2 == 0 ? 1 : -1); // Z: -50 ~ 50
    boids[i].vx =
        (rand() % 3 + 1) *
        (rand() % 2 == 0 ? 1 : -1); //(double)(rand() % 100 - 50) / 50.0;
    boids[i].vy =
        (rand() % 3 + 1) *
        (rand() % 2 == 0 ? 1 : -1); //(double)(rand() % 40 - 20) / 20.0;
    boids[i].vz =
        (rand() % 3 + 1) *
        (rand() % 2 == 0 ? 1 : -1); //(double)(rand() % 100 - 50) / 50.0;
  }
}

// 判斷boid
void update_boid(Boid boids[]) {
  int i;
  int neighbors = 0;
  for (i = 0; i < boid_num; i++) {
    double avg_vx = 0.0; // average
    double avg_vy = 0.0;
    double avg_vz = 0.0;

    double avg_xpos = 0.0;
    double avg_ypos = 0.0;
    double avg_zpos = 0.0;

    double close_dx = 0.0;
    double close_dy = 0.0;
    double close_dz = 0.0;

    double speed = 0.0;

    // alignment
    int j;
    for (j = 0; j < boid_num; j++) {
      if (i != j) {
        double dx = boids[j].x - boids[i].x;
        double dy = boids[j].y - boids[i].y;
        double dz = boids[j].z - boids[i].z;
        double distance = sqrt(dx * dx + dy * dy + dz * dz);
        if (distance < protected_range) { // seperation
          close_dx += boids[i].x - boids[j].x;
          close_dy += boids[i].y - boids[j].y;
          close_dz += boids[i].z - boids[j].z;

        } else if (distance < visual_range) { // in visual area
          // alignment
          avg_vx += boids[j].vx;
          avg_vy += boids[j].vy;
          avg_vz += boids[j].vz;
          // cohesion
          avg_xpos += boids[j].x;
          avg_ypos += boids[j].y;
          avg_zpos += boids[j].z;

          neighbors++;
        }
      }
    }
    if (neighbors > 0) {
      // alignment
      avg_vx = avg_vx / neighbors;
      avg_vy = avg_vy / neighbors;
      avg_vz = avg_vz / neighbors;
      boids[i].vx += (avg_vx - boids[i].vx) * matchingfactor;
      boids[i].vy += (avg_vy - boids[i].vy) * matchingfactor;
      boids[i].vz += (avg_vz - boids[i].vz) * matchingfactor;
      // cohesion
      avg_xpos = avg_xpos / neighbors;
      avg_ypos = avg_ypos / neighbors;
      avg_zpos = avg_zpos / neighbors;
      boids[i].vx += (avg_xpos - boids[i].x) * centeringfactor;
      boids[i].vy += (avg_ypos - boids[i].y) * centeringfactor;
      boids[i].vz += (avg_zpos - boids[i].z) * centeringfactor;
    }

    boids[i].vx += close_dx * avoidfactor;
    boids[i].vy += close_dy * avoidfactor;
    boids[i].vz += close_dz * avoidfactor;

    // boundary: X:-240 ~ 240 , Y:-200 ~ 200
    if (boids[i].x > x_boundary - 30) { // 70
      boids[i].vx -= turnfactor;
    }
    if (boids[i].x < -x_boundary + 30) { //-70
      boids[i].vx += turnfactor;
    }
    if (boids[i].y > y_boundary - 30) { // 70
      boids[i].vy -= turnfactor;
    }
    if (boids[i].y < -y_boundary + 30) { //-70
      boids[i].vy += turnfactor;
    }
    if (boids[i].z > z_boundary - 30) { // 70
      boids[i].vz -= turnfactor;
    }
    if (boids[i].z < -z_boundary + 30) { //-70
      boids[i].vz += turnfactor;
    }
    speed = sqrt(boids[i].vx * boids[i].vx + boids[i].vy * boids[i].vy +
                 boids[i].vz * boids[i].vz);
    if (speed < min_speed) {
      boids[i].vx = (boids[i].vx / speed) * min_speed;
      boids[i].vy = (boids[i].vy / speed) * min_speed;
      boids[i].vz = (boids[i].vz / speed) * min_speed;
    }
    if (speed > max_speed) {
      boids[i].vx = (boids[i].vx / speed) * max_speed;
      boids[i].vy = (boids[i].vy / speed) * max_speed;
      boids[i].vz = (boids[i].vz / speed) * max_speed;
    }
    boids[i].x = boids[i].x + boids[i].vx;
    boids[i].y = boids[i].y + boids[i].vy;
    boids[i].z = boids[i].z + boids[i].vz;
  }
}

void boids_get_position(Boid boids[], Position_3d boids_pos[]) {
  int i;
  for (i = 0; i < boid_num; i++) {
    boids_pos[i].x = boids[i].x;
    boids_pos[i].y = boids[i].y;
    boids_pos[i].z = boids[i].z;
  }
}

double adjust_boid_size(Position_3d boids_pos) {
  // 100~-100 -> 1.0~6.0
  double r;
  r = (-boids_pos.z / 40) + 3.5;
  // r = (-1 / 40 ) * boids_pos.z + 3.5;
  return r;
}

int main(void) {
  // === need to mmap: =======================
  // FPGA_CHAR_BASE
  // FPGA_ONCHIP_BASE
  // HW_REGS_BASE

  // === get FPGA addresses ==================
  // Open /dev/mem
  if ((fd = open("/dev/mem", (O_RDWR | O_SYNC))) == -1) {
    printf("ERROR: could not open \"/dev/mem\"...\n");
    return (1);
  }

  // get virtual addr that maps to physical
  h2p_lw_virtual_base = mmap(NULL, HW_REGS_SPAN, (PROT_READ | PROT_WRITE),
                             MAP_SHARED, fd, HW_REGS_BASE);
  if (h2p_lw_virtual_base == MAP_FAILED) {
    printf("ERROR: mmap1() failed...\n");
    close(fd);
    return (1);
  }

  // === get VGA char addr =====================
  // get virtual addr that maps to physical
  vga_char_virtual_base = mmap(NULL, FPGA_CHAR_SPAN, (PROT_READ | PROT_WRITE),
                               MAP_SHARED, fd, FPGA_CHAR_BASE);
  if (vga_char_virtual_base == MAP_FAILED) {
    printf("ERROR: mmap2() failed...\n");
    close(fd);
    return (1);
  }

  // Get the address that maps to the FPGA LED control
  vga_char_ptr = (unsigned int *)(vga_char_virtual_base);

  // === get VGA pixel addr ====================
  // get virtual addr that maps to physical
  vga_pixel_virtual_base = mmap(NULL, SDRAM_SPAN, (PROT_READ | PROT_WRITE),
                                MAP_SHARED, fd, SDRAM_BASE);
  if (vga_pixel_virtual_base == MAP_FAILED) {
    printf("ERROR: mmap3() failed...\n");
    close(fd);
    return (1);
  }

  // Get the address that maps to the FPGA pixel buffer
  vga_pixel_ptr = (unsigned int *)(vga_pixel_virtual_base);

  // ===========================================

  /* create a message to be displayed on the VGA
    and LCD displays */
  char text_top_row[40] = "DE1-SoC ARM/FPGA\0";
  char text_bottom_row[40] = "Cornell ece5760\0";
  char text_next[40] = "Graphics primitives\0";
  char num_string[20], time_string[20];
  char color_index = 0;
  int color_counter = 0;

  // position of disk primitive
  int disc_x = 0;
  // position of circle primitive
  int circle_x = 0;
  // position of box primitive
  int box_x = 5;
  // position of vertical line primitive
  int Vline_x = 350;
  // position of horizontal line primitive
  int Hline_y = 250;

  //  clear the screen
  VGA_box(0, 0, 639, 479, 0x0000);
  // clear the text
  VGA_text_clear();

  // ************************************************************************
  // Boid
  Boid boids[boid_num];
  Position_3d boids_pos[boid_num]; // rotated position
  Projection_3d boids_projected[boid_num];
  Boid boids_prev[boid_num];
  int boid_queue[boid_num] = {0}; // display queue
  double boid_size = 0.0;         // according to z
  // test 3D Rotation
  // Boid boid_test;
  // Position_3d boid_test_pos; // rotated position
  // Projection_3d boid_test_projected;
  // Boid boid_test_prev;

  srand(time(NULL));
  // initialize boid_test
  // boid_test.x = 0;
  // boid_test.y = 60;
  // boid_test.z = 0;
  // boid_test.vx = 0;
  // boid_test.vy = 0;
  // boid_test.vz = 0;
  // boid_test.color = gray;
  // boid_test.monitor_x = 0;
  // boid_test.monitor_y = 0;
  // boid_test_prev.monitor_x = boid_test.monitor_x;
  // boid_test_prev.monitor_y = boid_test.monitor_y;

  // boid_test_pos.x = boid_test.x;
  // boid_test_pos.y = boid_test.y;
  // boid_test_pos.z = boid_test.z;

  initialize_boid(boids);
  // cube projection on screen
  int v; // vertices
  int i; // variable for loop
  int l; // for axis
  set_nonblocking_mode();
  int c; // for keyboard control
  // boid_prev initialization (if didn't init, first cleaning will be bug)
  // but 1 boid can run when no boid_prev initialization, Idk why.
  for (i = 0; i < boid_num; i++) {
    boids_prev[i] = boids[i];
  }

  // for (v = 0; v < 8; v++) {
  //   printf("Vertex %d: x = %.2f, y = %.2f, z = %.2f\n", v,
  //   cube_vertices[v].x,
  //          cube_vertices[v].y, cube_vertices[v].z);
  // }
  // Position_3d rotated_v[vertices_num];
  Projection_3d projected_v[vertices_num];
  Projection_3d display_v[vertices_num];      // display on screen
  Projection_3d prev_display_v[vertices_num]; // previous

  // Position_3d rotated_axis[axis_num];
  Projection_3d projected_axis[axis_num];
  Projection_3d display_axis[axis_num];      // display on screen
  Projection_3d prev_display_axis[axis_num]; // previous
  rx = 0, ry = 0, rz = 0;

  // memcpy(boid_prev, boids, sizeof(Boid) * boid_num);
  while (1) {
    // start timer
    gettimeofday(&t1, NULL);
    // stop timer
    gettimeofday(&t2, NULL);
    elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000000.0; // sec to us
    elapsedTime += (t2.tv_usec - t1.tv_usec);          // us
    sprintf(time_string, "T = %6.0f uSec  ", elapsedTime);

    /*
    // boundary
    // outside
    VGA_Hline(0, 1, WIDTH, white);          // top
    VGA_Hline(0, HEIGHT - 1, WIDTH, white); // bottom
    VGA_Vline(4, 0, HEIGHT, white);         // left
    VGA_Vline(WIDTH - 1, 0, HEIGHT, white); // right
    // inside
    VGA_Hline(80, 60, WIDTH - 80, white);          // top
    VGA_Hline(80, HEIGHT - 60, WIDTH - 80, white); // bottom
    VGA_Vline(80, 60, HEIGHT - 60, white);         // left
    VGA_Vline(WIDTH - 80, 60, HEIGHT - 60, white); // right
    */

    update_boid(boids);
    boids_get_position(boids, boids_pos);
    // for (i = 0; i < boid_num; i++) {
    //   printf("boids: x: %f, y: %f, z: %f\n", boids[i].x, boids[i].y,
    //          boids[i].z);
    //   printf("boids_pos: x: %f, y: %f, z: %f\n", boids_pos[i].x,
    //   boids_pos[i].y,
    //          boids_pos[i].z);
    // }
    // boid_test.x += boid_test.vx;
    // boid_test.y += boid_test.vy;
    // boid_test.z += boid_test.vz;

    rx = rx * (M_PI / 180);
    ry = ry * (M_PI / 180);
    rz = rz * (M_PI / 180);
    // printf("rx: %f ry: %f rz:%f\n", rx, ry, rz);
    // Rotation
    if (rx != 0) {
      // Boids
      for (i = 0; i < boid_num; i++) {
        rotate_x(&boids_pos[i], rx);
      }
      // Cube
      for (v = 0; v < vertices_num; v++) {
        rotate_x(&cube_vertices[v], rx);
        // printf("rotated %d: x:%.4lf y:%.4lf z:%.4lf\n", v, rotated_v[v].x,
        //        rotated_v[v].y, rotated_v[v].z);
      }
      for (l = 0; l < 4; l++) {
        rotate_x(&axis_line[l], rx);
      }
      // Axes:X,Y,Z
      rx = 0;
    }
    if (ry != 0) {
      for (i = 0; i < boid_num; i++) {
        rotate_y(&boids_pos[i], ry);
      }
      for (v = 0; v < vertices_num; v++) {
        rotate_y(&cube_vertices[v], ry);
      }
      for (l = 0; l < 4; l++) {
        rotate_y(&axis_line[l], ry);
      }
      ry = 0;
    }
    if (rz != 0) {
      for (i = 0; i < boid_num; i++) {
        rotate_z(&boids_pos[i], rz);
      }
      for (v = 0; v < vertices_num; v++) {
        rotate_z(&cube_vertices[v], rz);
      }
      for (l = 0; l < 4; l++) {
        rotate_z(&axis_line[l], rz);
      }
      rz = 0;
    }
    // revise the position of boid
    for (i = 0; i < boid_num; i++) {
      boids[i].x = boids_pos[i].x;
      boids[i].y = boids_pos[i].y;
      boids[i].z = boids_pos[i].z;
    }

    qsort(boids_pos, boid_num, sizeof(Position_3d), compare_boids);

    // Projection
    // Boids
    for (i = 0; i < boid_num; i++) {
      boids_projected[i] = project(boids_pos[i], project_f, project_d);
      boids[i].monitor_x = boids_projected[i].x + x_coordinate_display;
      boids[i].monitor_y = boids_projected[i].y + y_coordinate_display;
      // printf("boid %d: x: %f y: %f z: %f\n", i, boids_pos[i].x, boids_pos[i].y,
      //        boids_pos[i].z);
    }
    // boid_test_projected = project(boid_test_pos, project_f, project_d);
    // boid_test.monitor_x = boid_test_projected.x + x_coordinate_display;
    // boid_test.monitor_y = boid_test_projected.y + y_coordinate_display;
    // Cube
    for (v = 0; v < vertices_num; v++) {
      projected_v[v] = project(cube_vertices[v], project_f, project_d);
      // printf("project: x:%lf y:%lf\n", projected_v[v].x, projected_v[v].y);
      // display on VGA
      display_v[v].x = projected_v[v].x + x_coordinate_display;
      display_v[v].y = projected_v[v].y + y_coordinate_display;
    }
    // Axes:X,Y,Z
    for (l = 0; l < 4; l++) {
      projected_axis[l] = project(axis_line[l], project_f, project_d);
      display_axis[l].x = projected_axis[l].x + x_coordinate_display;
      display_axis[l].y = projected_axis[l].y + y_coordinate_display;
    }
    for (i = 0; i < boid_num; i++) {
      boids[i].r = adjust_boid_size(boids_pos[i]);
      // printf("r: %f\n", boids[i].r);
    }
    // Clean previous points
    // Boids
    for (i = 0; i < boid_num; i++) {
      VGA_disc(boids_prev[i].monitor_x, boids_prev[i].monitor_y,
               boids_prev[i].r,
               black); // clean coordinate
      // printf("%f\n", boids[i].r);
    }
    // Boid_test_2
    // VGA_disc(boid_test_prev.monitor_x, boid_test_prev.monitor_y, 2, black);
    // temprarily suspend
    // Cube
    for (v = 0; v < vertices_num; v++) {
      VGA_disc(prev_display_v[v].x, prev_display_v[v].y, 2, black);
    }
    for (v = 0; v < line_num; v++) {
      VGA_line(prev_display_v[cube_edge[v][0]].x,
               prev_display_v[cube_edge[v][0]].y,
               prev_display_v[cube_edge[v][1]].x,
               prev_display_v[cube_edge[v][1]].y, black);
    }
    // Axes:X,Y,Z
    for (l = 0; l < 4; l++) {
      VGA_line(prev_display_axis[0].x, prev_display_axis[0].y,
               prev_display_axis[l].x, prev_display_axis[l].y, black);
    }

    // Display on screean
    // Boids
    for (i = 0; i < boid_num; i++) {
      VGA_disc(boids[i].monitor_x, boids[i].monitor_y, boids[i].r,
               boids[i].color); // draw coordinate
      boids_prev[i].monitor_x = boids[i].monitor_x;
      boids_prev[i].monitor_y = boids[i].monitor_y;
      boids_prev[i].r = boids[i].r;
    }
    // VGA_disc(boid_test.monitor_x, boid_test.monitor_y, 2, boid_test.color);
    // boid_test_prev.monitor_x = boid_test.monitor_x;
    // boid_test_prev.monitor_y = boid_test.monitor_y;
    // Cube
    for (v = 0; v < vertices_num; v++) {
      VGA_disc(display_v[v].x, display_v[v].y, 2, colors[0]);
      prev_display_v[v].x = display_v[v].x;
      prev_display_v[v].y = display_v[v].y;
    }
    for (v = 0; v < line_num; v++) {
      VGA_line(display_v[cube_edge[v][0]].x, display_v[cube_edge[v][0]].y,
               display_v[cube_edge[v][1]].x, display_v[cube_edge[v][1]].y,
               colors[6]);
    }
    // Axes:X,Y,Z
    for (l = 0; l < 4; l++) {
      VGA_line(display_axis[0].x, display_axis[0].y, display_axis[l].x,
               display_axis[l].y, colors[10]);
      prev_display_axis[l].x = display_axis[l].x;
      prev_display_axis[l].y = display_axis[l].y;
    }

    // Rotation Detections and Modifications
    // Keyboard Control
    c = getchar();
    // direction buttons
    if(c == '\033'){
      getchar();
      c = getchar();

      switch(c){
          case 'A': // up
            rx-=1;
            printf("-rx: %f ry: %f rz:%f\n", rx, ry, rz);
            break;
          case 'B': // down
            rx++;
            printf("-rx: %f ry: %f rz:%f\n", rx, ry, rz);
            break;
          case 'C': // right
            ry--;
            printf("-rx: %f ry: %f rz:%f\n", rx, ry, rz);
            break;
          case 'D': // left
            ry++;
            printf("-rx: %f ry: %f rz:%f\n", rx, ry, rz);
            break;
      }
      printf("--rx: %f ry: %f rz:%f\n", rx, ry, rz);
    }
    // rx -= 2;
    printf("rx: %f ry: %f rz:%f\n", rx, ry, rz);
    // ry += 2; // slowly rotate
    // rz += 2;
    if (rx >= 360) {
      rx = 0;
    }
    if (ry >= 360) {
      ry = 0;
    }
    if (rz >= 360) {
      rz = 0;
    }

    // set frame rate
    usleep(17000); // 17000
                   // sleep(0.25);
                   //  clear the screen
                   // VGA_box(0, 0, 639, 479, 0x0000);
  }                // end while(1)
} // end main

/****************************************************************************************
 * Subroutine to send a string of text to the VGA monitor
 ****************************************************************************************/
void VGA_text(int x, int y, char *text_ptr) {
  volatile char *character_buffer =
      (char *)vga_char_ptr; // VGA character buffer
  int offset;
  /* assume that the text string fits on one line */
  offset = (y << 7) + x;
  while (*(text_ptr)) {
    // write to the character buffer
    *(character_buffer + offset) = *(text_ptr);
    ++text_ptr;
    ++offset;
  }
}

/****************************************************************************************
 * Subroutine to clear text to the VGA monitor
 ****************************************************************************************/
void VGA_text_clear() {
  volatile char *character_buffer =
      (char *)vga_char_ptr; // VGA character buffer
  int offset, x, y;
  for (x = 0; x < 79; x++) {
    for (y = 0; y < 59; y++) {
      /* assume that the text string fits on one line */
      offset = (y << 7) + x;
      // write to the character buffer
      *(character_buffer + offset) = ' ';
    }
  }
}

/****************************************************************************************
 * Draw a filled rectangle on the VGA monitor
 ****************************************************************************************/
#define SWAP(X, Y)                                                             \
  do {                                                                         \
    int temp = X;                                                              \
    X = Y;                                                                     \
    Y = temp;                                                                  \
  } while (0)

void VGA_box(int x1, int y1, int x2, int y2, short pixel_color) {
  char *pixel_ptr;
  int row, col;

  /* check and fix box coordinates to be valid */
  if (x1 > 639)
    x1 = 639;
  if (y1 > 479)
    y1 = 479;
  if (x2 > 639)
    x2 = 639;
  if (y2 > 479)
    y2 = 479;
  if (x1 < 0)
    x1 = 0;
  if (y1 < 0)
    y1 = 0;
  if (x2 < 0)
    x2 = 0;
  if (y2 < 0)
    y2 = 0;
  if (x1 > x2)
    SWAP(x1, x2);
  if (y1 > y2)
    SWAP(y1, y2);
  for (row = y1; row <= y2; row++)
    for (col = x1; col <= x2; ++col) {
      // 640x480
      // pixel_ptr = (char *)vga_pixel_ptr + (row<<10)    + col ;
      //  set pixel color
      //*(char *)pixel_ptr = pixel_color;
      VGA_PIXEL(col, row, pixel_color);
    }
}

/****************************************************************************************
 * Draw a outline rectangle on the VGA monitor
 ****************************************************************************************/
#define SWAP(X, Y)                                                             \
  do {                                                                         \
    int temp = X;                                                              \
    X = Y;                                                                     \
    Y = temp;                                                                  \
  } while (0)

void VGA_rect(int x1, int y1, int x2, int y2, short pixel_color) {
  char *pixel_ptr;
  int row, col;

  /* check and fix box coordinates to be valid */
  if (x1 > 639)
    x1 = 639;
  if (y1 > 479)
    y1 = 479;
  if (x2 > 639)
    x2 = 639;
  if (y2 > 479)
    y2 = 479;
  if (x1 < 0)
    x1 = 0;
  if (y1 < 0)
    y1 = 0;
  if (x2 < 0)
    x2 = 0;
  if (y2 < 0)
    y2 = 0;
  if (x1 > x2)
    SWAP(x1, x2);
  if (y1 > y2)
    SWAP(y1, y2);
  // left edge
  col = x1;
  for (row = y1; row <= y2; row++) {
    // 640x480
    // pixel_ptr = (char *)vga_pixel_ptr + (row<<10)    + col ;
    //  set pixel color
    //*(char *)pixel_ptr = pixel_color;
    VGA_PIXEL(col, row, pixel_color);
  }

  // right edge
  col = x2;
  for (row = y1; row <= y2; row++) {
    // 640x480
    // pixel_ptr = (char *)vga_pixel_ptr + (row<<10)    + col ;
    //  set pixel color
    //*(char *)pixel_ptr = pixel_color;
    VGA_PIXEL(col, row, pixel_color);
  }

  // top edge
  row = y1;
  for (col = x1; col <= x2; ++col) {
    // 640x480
    // pixel_ptr = (char *)vga_pixel_ptr + (row<<10)    + col ;
    //  set pixel color
    //*(char *)pixel_ptr = pixel_color;
    VGA_PIXEL(col, row, pixel_color);
  }

  // bottom edge
  row = y2;
  for (col = x1; col <= x2; ++col) {
    // 640x480
    // pixel_ptr = (char *)vga_pixel_ptr + (row<<10)    + col ;
    //  set pixel color
    //*(char *)pixel_ptr = pixel_color;
    VGA_PIXEL(col, row, pixel_color);
  }
}

/****************************************************************************************
 * Draw a horixontal line on the VGA monitor
 ****************************************************************************************/
#define SWAP(X, Y)                                                             \
  do {                                                                         \
    int temp = X;                                                              \
    X = Y;                                                                     \
    Y = temp;                                                                  \
  } while (0)

void VGA_Hline(int x1, int y1, int x2, short pixel_color) {
  char *pixel_ptr;
  int row, col;

  /* check and fix box coordinates to be valid */
  if (x1 > 639)
    x1 = 639;
  if (y1 > 479)
    y1 = 479;
  if (x2 > 639)
    x2 = 639;
  if (x1 < 0)
    x1 = 0;
  if (y1 < 0)
    y1 = 0;
  if (x2 < 0)
    x2 = 0;
  if (x1 > x2)
    SWAP(x1, x2);
  // line
  row = y1;
  for (col = x1; col <= x2; ++col) {
    // 640x480
    // pixel_ptr = (char *)vga_pixel_ptr + (row<<10)    + col ;
    //  set pixel color
    //*(char *)pixel_ptr = pixel_color;
    VGA_PIXEL(col, row, pixel_color);
  }
}

/****************************************************************************************
 * Draw a vertical line on the VGA monitor
 ****************************************************************************************/
#define SWAP(X, Y)                                                             \
  do {                                                                         \
    int temp = X;                                                              \
    X = Y;                                                                     \
    Y = temp;                                                                  \
  } while (0)

void VGA_Vline(int x1, int y1, int y2, short pixel_color) {
  char *pixel_ptr;
  int row, col;

  /* check and fix box coordinates to be valid */
  if (x1 > 639)
    x1 = 639;
  if (y1 > 479)
    y1 = 479;
  if (y2 > 479)
    y2 = 479;
  if (x1 < 0)
    x1 = 0;
  if (y1 < 0)
    y1 = 0;
  if (y2 < 0)
    y2 = 0;
  if (y1 > y2)
    SWAP(y1, y2);
  // line
  col = x1;
  for (row = y1; row <= y2; row++) {
    // 640x480
    // pixel_ptr = (char *)vga_pixel_ptr + (row<<10)    + col ;
    //  set pixel color
    //*(char *)pixel_ptr = pixel_color;
    VGA_PIXEL(col, row, pixel_color);
  }
}

/****************************************************************************************
 * Draw a filled circle on the VGA monitor
 ****************************************************************************************/

void VGA_disc(int x, int y, int r, short pixel_color) {
  char *pixel_ptr;
  int row, col, rsqr, xc, yc;

  rsqr = r * r;

  for (yc = -r; yc <= r; yc++)
    for (xc = -r; xc <= r; xc++) {
      col = xc;
      row = yc;
      // add the r to make the edge smoother
      if (col * col + row * row <= rsqr + r) {
        col += x; // add the center point
        row += y; // add the center point
        // check for valid 640x480
        if (col > 639)
          col = 639;
        if (row > 479)
          row = 479;
        if (col < 0)
          col = 0;
        if (row < 0)
          row = 0;
        // pixel_ptr = (char *)vga_pixel_ptr + (row<<10) + col ;
        //  set pixel color
        //*(char *)pixel_ptr = pixel_color;
        VGA_PIXEL(col, row, pixel_color);
      }
    }
}

/****************************************************************************************
 * Draw a  circle on the VGA monitor
 ****************************************************************************************/

void VGA_circle(int x, int y, int r, int pixel_color) {
  char *pixel_ptr;
  int row, col, rsqr, xc, yc;
  int col1, row1;
  rsqr = r * r;

  for (yc = -r; yc <= r; yc++) {
    // row = yc;
    col1 = (int)sqrt((float)(rsqr + r - yc * yc));
    // right edge
    col = col1 + x; // add the center point
    row = yc + y;   // add the center point
    // check for valid 640x480
    if (col > 639)
      col = 639;
    if (row > 479)
      row = 479;
    if (col < 0)
      col = 0;
    if (row < 0)
      row = 0;
    // pixel_ptr = (char *)vga_pixel_ptr + (row<<10) + col ;
    //  set pixel color
    //*(char *)pixel_ptr = pixel_color;
    VGA_PIXEL(col, row, pixel_color);
    // left edge
    col = -col1 + x; // add the center point
    // check for valid 640x480
    if (col > 639)
      col = 639;
    if (row > 479)
      row = 479;
    if (col < 0)
      col = 0;
    if (row < 0)
      row = 0;
    // pixel_ptr = (char *)vga_pixel_ptr + (row<<10) + col ;
    //  set pixel color
    //*(char *)pixel_ptr = pixel_color;
    VGA_PIXEL(col, row, pixel_color);
  }
  for (xc = -r; xc <= r; xc++) {
    // row = yc;
    row1 = (int)sqrt((float)(rsqr + r - xc * xc));
    // right edge
    col = xc + x;   // add the center point
    row = row1 + y; // add the center point
    // check for valid 640x480
    if (col > 639)
      col = 639;
    if (row > 479)
      row = 479;
    if (col < 0)
      col = 0;
    if (row < 0)
      row = 0;
    // pixel_ptr = (char *)vga_pixel_ptr + (row<<10) + col ;
    //  set pixel color
    //*(char *)pixel_ptr = pixel_color;
    VGA_PIXEL(col, row, pixel_color);
    // left edge
    row = -row1 + y; // add the center point
    // check for valid 640x480
    if (col > 639)
      col = 639;
    if (row > 479)
      row = 479;
    if (col < 0)
      col = 0;
    if (row < 0)
      row = 0;
    // pixel_ptr = (char *)vga_pixel_ptr + (row<<10) + col ;
    //  set pixel color
    //*(char *)pixel_ptr = pixel_color;
    VGA_PIXEL(col, row, pixel_color);
  }
}

// =============================================
// === Draw a line
// =============================================
// plot a line
// at x1,y1 to x2,y2 with color
// Code is from David Rodgers,
//"Procedural Elements of Computer Graphics",1985
void VGA_line(int x1, int y1, int x2, int y2, short c) {
  int e;
  signed int dx, dy, j, temp;
  signed int s1, s2, xchange;
  signed int x, y;
  char *pixel_ptr;

  /* check and fix line coordinates to be valid */
  if (x1 > 639)
    x1 = 639;
  if (y1 > 479)
    y1 = 479;
  if (x2 > 639)
    x2 = 639;
  if (y2 > 479)
    y2 = 479;
  if (x1 < 0)
    x1 = 0;
  if (y1 < 0)
    y1 = 0;
  if (x2 < 0)
    x2 = 0;
  if (y2 < 0)
    y2 = 0;

  x = x1;
  y = y1;

  // take absolute value
  if (x2 < x1) {
    dx = x1 - x2;
    s1 = -1;
  }

  else if (x2 == x1) {
    dx = 0;
    s1 = 0;
  }

  else {
    dx = x2 - x1;
    s1 = 1;
  }

  if (y2 < y1) {
    dy = y1 - y2;
    s2 = -1;
  }

  else if (y2 == y1) {
    dy = 0;
    s2 = 0;
  }

  else {
    dy = y2 - y1;
    s2 = 1;
  }

  xchange = 0;

  if (dy > dx) {
    temp = dx;
    dx = dy;
    dy = temp;
    xchange = 1;
  }

  e = ((int)dy << 1) - dx;

  for (j = 0; j <= dx; j++) {
    // video_pt(x,y,c); //640x480
    // pixel_ptr = (char *)vga_pixel_ptr + (y<<10)+ x;
    //  set pixel color
    //*(char *)pixel_ptr = c;
    VGA_PIXEL(x, y, c);

    if (e >= 0) {
      if (xchange == 1)
        x = x + s1;
      else
        y = y + s2;
      e = e - ((int)dx << 1);
    }

    if (xchange == 1)
      y = y + s2;
    else
      x = x + s1;

    e = e + ((int)dy << 1);
  }
}
