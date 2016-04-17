# -*- coding: utf-8 -*-

# kör like so: python has_action.py ../bilder/16-25-59.jpeg ../bilder/16-26-02.jpeg ../bilder/16-26-04.jpeg

# bilder ovan resulterar i norm 0.075
# tar man istället bilder där ena har fågel typ
# ../bilder/16-30-12.jpeg ../bilder/16-30-14.jpeg ../bilder/16-30-16.jpeg
# så får man istället 0.362, woohooooo!

import sys

import time as time

from scipy import misc
from PIL import Image

import numpy as np
import matplotlib.pyplot as plt

class ActionRecognizer:

    def __init__(self, size=0.10, alpha=0.2):
        self.size = size
        self.alpha = alpha

    def _format_img(self, raw_img):
        unresized_img = np.array(raw_img.convert('L')) # konv till monochrome
        return misc.imresize(unresized_img, self.size) / 255.

    def _single_is_different(self, img1, img2):
        diff = np.absolute(self._format_img(img2) - self._format_img(img1))
        # # debugging-rader, men kul att kolla paw
        # print "nooorm is "
        # print np.linalg.norm(diff)
        # plt.imshow(diff, cmap=plt.cm.gray)
        # plt.show()
        return np.linalg.norm(diff) > self.alpha
    
    def _is_different(self, img, img_before, img_after):
        return self._single_is_different(img, img_before) and\
                self._single_is_different(img, img_after)

    def _import_image(self, path):
        return Image.open(path)

    def find_action(self, paths):
        """
        Returnerar en lista med de sökvägar i listan paths som innehåller
        bilder med mycket action. En bild måste vara annorlunda både
        jämfört med bilden innan och bilden efter för att anses innehålla
        action.
        """
        has_action = []
        images = []

        for i in range(0,len(paths)):
            images.append(self._import_image(paths[i]))

        for i in range(1,len(paths)-1):
            if self._is_different(images[i], images[i-1], images[i+1]):
                has_action.append(paths[i])

        return has_action

start_time = time.time()

argvs = []
for i in range(1,len(sys.argv)):
    argvs.append(sys.argv[i])

recognizer = ActionRecognizer()
print recognizer.find_action(argvs)

print time.time() - start_time

