import pygame
import pygame.camera
from pygame.locals import *
from PIL import Image

pygame.init()
pygame.camera.init()
w = 640
h = 480
size=(w,h)
screen = pygame.display.set_mode(size)

cam = pygame.camera.Camera("/dev/video0",(640,480))
cam.start()
image = cam.get_image()

screen.blit(image,(0,0))
pygame.display.flip()