#!/usr/bin/env python3
import argparse
import json
import xml.etree.ElementTree as xml
import xml.dom.minidom as xml_format

def main():
    description = "Generate Batsim heterogeneous platforms"
    ap = argparse.ArgumentParser(description=description)

    ap.add_argument("-p", "--platform-file", type=str, required=True,
                    help="JSON with platform description")
    ap.add_argument("-o", "--output-xml", type=str, default="platform.xml",
                    help="XML output with platform ready for Batsim")
    args = ap.parse_args()

    with open(args.output_xml, "w") as output_f,\
         open(args.platform_file, "r") as platform_f,\
         open("network_types.json", "r") as network_types_f,\
         open("node_types.json", "r") as node_types_f,\
         open("processor_types.json", "r") as processor_types_f:
        platform = json.load(platform_f)
        network_types = json.load(network_types_f)
        node_types = json.load(node_types_f)
        processor_types = json.load(processor_types_f)
        # DOCTYPE specification
        doctype = "<!DOCTYPE platform SYSTEM \"https://simgrid.org/simgrid.dtd\">"
        # Platform
        platform_attrs = {"version": "4.1"}
        platform_el = xml.Element("platform", attrib=platform_attrs)
        # Main zone
        main_zone_attrs = {"id": "main", "routing": "Full"}
        main_zone_el = xml.SubElement(platform_el, "zone", attrib=main_zone_attrs)
        # Master zone and master host
        master_zone_attrs = {"id": "master", "routing": "None"}
        master_zone_el = xml.SubElement(main_zone_el, "zone", attrib=master_zone_attrs)
        mhost_attrs = {"id": "master_host", "speed": "1Gf"}
        xml.SubElement(master_zone_el, "host", attrib=mhost_attrs)
        # User config zone
        config_zone_attrs = {"id": "config", "routing": "None"}
        config_zone_el = xml.SubElement(main_zone_el, "zone", attrib=config_zone_attrs)
        # Clusters
        recorded_nodes = {}
        cluster_idx = 0
        for cluster in platform["clusters"]:
            # Cluster
            cluster_id = "clu_{}".format(cluster_idx)
            cluster_attrs = {"id": cluster_id, "routing": "Cluster"}
            cluster_el = xml.SubElement(main_zone_el, "zone", attrib=cluster_attrs)
            # Nodes
            for node in cluster["nodes"]:
                node_template = node_types[node["type"]]
                # Memory info
                if node["type"] not in recorded_nodes:
                    mem_id = "mem_{}".format(node_template["id"])
                    mem_attrs = {"id": mem_id, "value": node_template["memory_gib"]}
                    xml.SubElement(config_zone_el, "prop", attrib=mem_attrs)
                for node_idx in range(int(node["number"])):
                    node_id = "{}_{}_{}".format(node_template["id"], node_idx, cluster_id)
                    # Up / down link
                    udlink_id = "udl_{}".format(node_id)
                    udlink_attrs = {"id": udlink_id, "sharing_policy": "SHARED"}
                    udlink_attrs.update(network_types[cluster["cluster_network"]])
                    xml.SubElement(cluster_el, "link", attrib=udlink_attrs)
                    # Processors
                    for proc in node_template["processors"]:
                        proc_template = processor_types[proc["type"]][proc["model"]]
                        for proc_idx in range(int(proc["number"])):
                            proc_id = "{}_{}_{}".format(proc_template["id"], proc_idx, node_id)
                            # Cores
                            for core_idx in range(int(proc_template["nb_cores"])):
                                core_id = "cor_{}_{}".format(core_idx, proc_id)
                                core_attrs = {"id": core_id}
                                core_attrs.update(proc_template["core_attributes"])
                                core_el = xml.SubElement(cluster_el, "host", attrib=core_attrs)
                                for prop in proc_template["core_properties"]:
                                    xml.SubElement(core_el, "prop", attrib=prop)
                                # Link association
                                hlink_attrs = {"id": core_id, "up": udlink_id, "down": udlink_id}
                                xml.SubElement(cluster_el, "host_link", attrib=hlink_attrs)
            # Router
            router_id = "rou_{}".format(cluster_idx)
            router_attrs = {"id": router_id}
            xml.SubElement(cluster_el, "router", attrib=router_attrs)
            # Backbone network
            backbone_id = "bbo_{}".format(cluster_idx)
            backbone_attrs = {"id": backbone_id}
            backbone_attrs.update(network_types[cluster["cluster_network"]])
            xml.SubElement(cluster_el, "backbone", attrib=backbone_attrs)
            cluster_idx += 1

        # Links from clusters to master host
        # Required to be stated after all clusters have been configured
        cluster_idx = 0
        for cluster in platform["clusters"]:
            cluster_id = "clu_{}".format(cluster_idx)
            ctmhlink_id = "tomh_{}".format(cluster_id)
            ctmhlink_attrs = {"id": ctmhlink_id}
            ctmhlink_attrs.update(network_types[platform["dc_network"]])
            xml.SubElement(main_zone_el, "link", attrib=ctmhlink_attrs)
            cluster_idx += 1

        # Routes to master zone
        # Required to be stated after all links have been configured
        cluster_idx = 0
        for cluster in platform["clusters"]:
            cluster_id = "clu_{}".format(cluster_idx)
            router_id = "rou_{}".format(cluster_idx)
            ctmhlink_id = "tomh_{}".format(cluster_id)
            route_attrs = {"src": cluster_id, "dst": "master", "gw_src": router_id, "gw_dst": "master_host"}
            route_el = xml.SubElement(main_zone_el, "zoneRoute", attrib=route_attrs)
            # Link association inside route
            xml.SubElement(route_el, "link_ctn", attrib={"id": ctmhlink_id})
            cluster_idx += 1

        # Write the output
        output_xml = xml_format.parseString("{}{}".format(doctype, xml.tostring(platform_el).decode()))
        output_f.write(output_xml.toprettyxml(indent="  ", encoding="utf-8").decode())

if __name__ == "__main__":
    main()
