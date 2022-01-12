from compute_buckets import *

import matplotlib.pyplot as plt
import numpy as np

from scipy.spatial import KDTree

plt.rcParams.update({
    'figure.figsize': (11, 7),
    'axes.xmargin': 0,
    'font.size': 16,
    'axes.grid.which': 'both'
})

N_BUCKETS = 10
N_MAX_LINES = 500000
N_STEP = 50000

if __name__ == "__main__":
    # standard equity bucketing against N ranges
    streets_to_bucket = [
         ('', 'turn', False, N_BUCKETS),
         ('', 'river', False, N_BUCKETS),
    ]

    for prefix, street, has_id, n in streets_to_bucket:
        print("Loading equities...")
        points = loadfile(os.path.join(DATA_PATH, f'{prefix}{street}_equities.txt'),
                            has_id, N_MAX_LINES)
        if has_id:
            ids, points = points
        print("Computing k-means...")

        clusters = []

        sample_steps = list(range(N_STEP, N_MAX_LINES+N_STEP, N_STEP))
        # compute clusters at various numbers of equity samples
        for n in tqdm(sample_steps):
            kmeans = MiniBatchKMeans(n_clusters=N_BUCKETS, random_state=0, verbose=0).fit(points[:n])

            clusters += [np.array(kmeans.cluster_centers_)]
        
        
        kd_tree = KDTree(clusters[-1])

        # match up the clusters at each step with the final clusters
        # for a distance comparison
        distances = np.empty((0, N_BUCKETS))
        for i, c in enumerate(clusters):
            dist, inds = kd_tree.query(c)
            clusters[i] = c[inds]
            distances = np.vstack((distances, dist[inds]))

        plt.figure()
        plt.plot(sample_steps, distances, '.-')
        plt.ylabel('Distance from cluster in final iteration')
        plt.xlabel('Number of equity samples used')
        plt.show()