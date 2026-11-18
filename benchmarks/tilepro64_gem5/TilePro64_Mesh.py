# Copyright (c) 2026 HPC Locality Project
# Custom 2D Mesh Topology for TILEPro Manycore in gem5 (Supports 4x4 Mini-Mesh and 8x8 Full Mesh)
# Topology: Routers, L1/L2 Caches, Perimeter Memory Controllers

from m5.params import *
from m5.objects import *
from topologies.BaseTopology import SimpleTopology

class TilePro64_Mesh(SimpleTopology):
    description = "TilePro64_Mesh"

    def __init__(self, controllers):
        self.nodes = controllers

    def makeTopology(self, options, network, IntLink, ExtLink, Router):
        nodes = self.nodes

        # 1. Categorize controllers
        l1_cntrls = []
        l2_cntrls = []
        dir_nodes = []
        dma_nodes = []

        for node in nodes:
            if node.type == 'L1Cache_Controller':
                l1_cntrls.append(node)
            elif node.type == 'L2Cache_Controller':
                l2_cntrls.append(node)
            elif node.type == 'Directory_Controller':
                dir_nodes.append(node)
            elif node.type == 'DMA_Controller':
                dma_nodes.append(node)

        num_routers = len(l1_cntrls)
        assert num_routers in [16, 64], f"Supported configurations are 16 (4x4) or 64 (8x8) tiles, found {num_routers}"
        assert len(l2_cntrls) == num_routers, f"Expected {num_routers} L2s, found {len(l2_cntrls)}"

        if num_routers == 16:
            num_rows = 4
            num_columns = 4
            # Perimeter routers for memory controllers: e.g. routers 4, 7 or 4, 7, 8, 11
            if len(dir_nodes) == 2:
                mc_routers = [4, 11]
            elif len(dir_nodes) == 4:
                mc_routers = [4, 7, 8, 11]
            else:
                mc_routers = [i % 16 for i in range(len(dir_nodes))]
        else:
            num_rows = 8
            num_columns = 8
            if len(dir_nodes) == 4:
                mc_routers = [16, 23, 40, 47]
            else:
                mc_routers = [i % 64 for i in range(len(dir_nodes))]

        link_latency = options.link_latency
        router_latency = options.router_latency

        # 2. Instantiate Garnet Routers
        routers = [Router(router_id=i, latency=router_latency) for i in range(num_routers)]
        network.routers = routers

        link_count = 0
        ext_links = []

        # 3. Connect L1 and L2 controllers to co-located Router i
        for i in range(num_routers):
            # L1 to Router i
            ext_links.append(ExtLink(link_id=link_count, ext_node=l1_cntrls[i],
                                     int_node=routers[i], latency=link_latency))
            link_count += 1
            # L2 (Home Bank) to Router i
            ext_links.append(ExtLink(link_id=link_count, ext_node=l2_cntrls[i],
                                     int_node=routers[i], latency=link_latency))
            link_count += 1

        # 4. Attach Memory Controllers to Perimeter Routers
        for mc_idx, r_idx in enumerate(mc_routers):
            ext_links.append(ExtLink(link_id=link_count, ext_node=dir_nodes[mc_idx],
                                     int_node=routers[r_idx], latency=link_latency))
            link_count += 1

        network.ext_links = ext_links

        # 5. Create 2D Mesh XY Internal Links (Weight 1 for X, Weight 2 for Y)
        int_links = []
        for y in range(num_rows):
            for x in range(num_columns):
                cur = y * num_columns + x

                # East-West Links
                if x + 1 < num_columns:
                    east = y * num_columns + (x + 1)
                    int_links.append(IntLink(link_id=link_count, src_node=routers[cur],
                                             dst_node=routers[east], latency=link_latency, weight=1))
                    link_count += 1
                    int_links.append(IntLink(link_id=link_count, src_node=routers[east],
                                             dst_node=routers[cur], latency=link_latency, weight=1))
                    link_count += 1

                # North-South Links
                if y + 1 < num_rows:
                    south = (y + 1) * num_columns + x
                    int_links.append(IntLink(link_id=link_count, src_node=routers[cur],
                                             dst_node=routers[south], latency=link_latency, weight=2))
                    link_count += 1
                    int_links.append(IntLink(link_id=link_count, src_node=routers[south],
                                             dst_node=routers[cur], latency=link_latency, weight=2))
                    link_count += 1

        network.int_links = int_links
