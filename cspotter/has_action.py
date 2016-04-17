# -*- coding: utf-8 -*-

# kör like so: python has_action.py bilder/16-25-59.jpeg bilder/16-26-02.jpeg

# bilder ovan resulterar i norm 0.075
# tar man istället bilder där ena har fågel typ
# bilder/16-30-12.jpeg bilder/16-30-14.jpeg
# så får man istället 0.362, woohooooo!

import sys

import time as time

from scipy import misc
from PIL import Image

import numpy as np
import matplotlib.pyplot as plt

start_time = time.time()

img1 = np.array(Image.open(sys.argv[1]).convert('L'))
img2 = np.array(Image.open(sys.argv[2]).convert('L'))

img1 = misc.imresize(img1, 0.10) / 255. # ger size (28, 35)
img2 = misc.imresize(img2, 0.10) / 255.

diff_img = img2 - img1

norm = np.linalg.norm(diff_img) # tar tydligen Frobeniusnormen, vettigt?
print norm

print time.time() - start_time

plt.imshow(diff_img, cmap=plt.cm.gray)
plt.show()
