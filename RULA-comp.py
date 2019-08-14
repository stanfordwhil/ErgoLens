import tinyik
import numpy as np
arm = tinyik.Actuator(['z', [380.337,147.999,0.834929], 'z', [369.875,202.756,0.828784]])
arm.ee = [471.672,252.382,0.799852]
print(np.round(np.rad2deg(arm.angles)))
