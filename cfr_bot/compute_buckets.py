import os
from sklearn.cluster import MiniBatchKMeans
from tqdm import tqdm

DATA_PATH = '../../data/equity_data/'

N_BUCKETS = 150

def loadfile(filename, has_id=True):
    ids = []
    points = []
    with open(filename) as file:
        for line in tqdm(file):
            l = line.replace('\n', '').split(' ')
            if len(l) > 0:
                if has_id:
                    points += [[float(x) for x in l[1:]]]
                    ids += [int(l[0])]
                else:
                    points += [[float(x) for x in l]]
    
    if has_id:
        return ids, points
    else:
        return points

def savefile_labels(ids, labels, filename):
    with open(filename, 'w') as file:
        for id, label in zip(ids, labels):
            file.write(f'{id} {label}\n')

def savefile_clusters(clusters, filename):
    with open(filename, 'w') as file:
        for c in clusters:
            file.write(' '.join([str(x) for x in c]) + '\n')

if __name__ == "__main__":
    # standard equity bucketing against N ranges
    streets_to_bucket = [
        ('', 'flop', True, N_BUCKETS),
         ('', 'turn', False, N_BUCKETS),
         ('', 'river', False, N_BUCKETS),
    ]

    for prefix, street, has_id, n in streets_to_bucket:
        print("Loading equities...")
        points = loadfile(os.path.join(DATA_PATH, f'{prefix}{street}_equities.txt'), has_id)
        if has_id:
            ids, points = points
        print("Computing k-means...")
        kmeans = MiniBatchKMeans(n_clusters=n, random_state=0, verbose=1).fit(points)

        if has_id:
            savefile_labels(ids, kmeans.labels_, os.path.join(DATA_PATH, f'{street}_buckets_{n}.txt'))
        savefile_clusters(kmeans.cluster_centers_, os.path.join(DATA_PATH, f'{street}_clusters_{n}.txt'))
