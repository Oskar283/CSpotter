#!/usr/bin/env python

import pygame.camera
import pygame.image

pygame.camera.init()
cam = pygame.camera.Camera(pygame.camera.list_cameras()[0])
cam.start()
img = cam.get_image()
pygame.image.save(img, "photo.bmp")
pygame.camera.quit()
