#!/usr/bin/env python3
import argparse
import json
import defusedxml.minidom as xml_format
from defusedxml.lxml import _etree as xml

def main():
    """
    Transform an heterogeneous platform description into a valid Batsim XML.
    """

    def cmd_args():
        """
        Parses command line arguments.
        """

        ap = argparse.ArgumentParser(description="Generate Batsim heterogeneous platforms")
        ap.add_argument("-p", "--platform-file", type=str, required=True,
                        help="JSON with platform description")
        ap.add_argument("-o", "--output-xml", type=str, default="platform.xml",
                        help="XML output with platform ready for Batsim")
        return ap.parse_args()

    def load_data():
        """
        Loads data for user's platform, network, node and processor types.
        """

        with open(args.platform_file, "r") as platform_f,\
             open("network_types.json", "r") as network_types_f,\
             open("node_types.json", "r") as node_types_f,\
             open("processor_types.json", "r") as processor_types_f:
            data = (json.load(platform_f), json.load(network_types_f),
                    json.load(node_types_f), json.load(processor_types_f))
        return data

    def generate_tree():
        """
        Creates an XML tree complying to SimGrid DTD.
        """

        def main_zone():
            """
            Contains master zone and all clusters.
            """

            return xml.SubElement(platform_xml, "zone",
                attrib={"id": "main", "routing": "Full"})

        def master_zone():
            """
            Hosts the master node which schedules jobs onto resources.
            """

            def master_host():
                """
                Executes the scheduling algorithms.
                """

                xml.SubElement(master_zone_xml, "host",
                    attrib={"id": "master_host", "speed": "1Gf"})

            master_zone_xml = xml.SubElement(main_zone_xml, "zone",
                attrib={"id": "master", "routing": "None"})
            master_host()

        def config_node():
            """
            Holds user defined properties concerning node types.
            """

            def config_zone():
                """
                Define node and proc config properties.
                """
                return xml.SubElement(main_zone_xml, "zone",
                    attrib={"id": "config", "routing": "None"})

            return xml.SubElement(config_zone(), "zone",
                attrib={"id": "node", "routing": "None"})

        def clusters():
            """
            Groups of nodes inside the data centre.
            """

            def nodes():
                """
                Systems available in the data centre, contain processors and other resources (v. gr. memory).
                They are connected to a common cluster backbone by up / down links.
                """

                def record_node_type():
                    """
                    Inserts the node type in the already configured ones.
                    """

                    if node["type"] not in recorded_nodes:
                        config_node_type_xml = xml.SubElement(config_node_xml, "zone",
                            attrib={"id": node["type"], "routing": "None"})
                        xml.SubElement(config_node_type_xml, "prop",
                            attrib={"id": "memory", "value": node_template["memory_gib"]})
                        recorded_nodes[node["type"]] = True

                def udlink():
                    """
                    Link between the node and the backbone.
                    """

                    udlink_id = "udl_{}".format(node_id)
                    _udlink_attrs = {"id": udlink_id, "sharing_policy": "SHARED"}
                    _udlink_attrs.update(network_types[cluster["cluster_network"]])
                    xml.SubElement(cluster_xml, "link", attrib=_udlink_attrs)
                    return udlink_id

                def procs():
                    """
                    Computing resources available in the data centre. These can be CPUs, GPUs, MICs, ...
                    They have a set of cores and power consumption properties.
                    """

                    def cores():
                        """
                        Individual computing units inside a processor.
                        """

                        def core_properties():
                            """
                            Defines node type and power consumption properties.
                            """

                            xml.SubElement(core_xml, "prop",
                                attrib={"id": "node_type", "value": node["type"]})
                            for prop in proc_template["core_properties"]:
                                xml.SubElement(core_xml, "prop", attrib=prop)

                        def link_association():
                            """
                            Associates up / down link with the core.
                            """

                            xml.SubElement(cluster_xml, "host_link",
                                attrib={"id": core_id, "up": udlink_id, "down": udlink_id})

                        for core_idx in range(int(proc_template["nb_cores"])):
                            core_id = "cor_{}_{}".format(core_idx, proc_id)
                            _core_attrs = {"id": core_id}
                            _core_attrs.update(proc_template["core_attributes"])
                            core_xml = xml.SubElement(cluster_xml, "host", attrib=_core_attrs)
                            core_properties()
                            link_association()

                    for proc in node_template["processors"]:
                        proc_template = processor_types[proc["type"]][proc["model"]]
                        for proc_idx in range(int(proc_template["nb_cores"])):
                            proc_id = "{}_{}_{}".format(proc_template["id"], proc_idx, node_id)
                            cores()

                for node in cluster["nodes"]:
                    node_template = node_types[node["type"]]
                    record_node_type()
                    for node_idx in range(int(node["number"])):
                        node_id = "{}_{}_{}".format(node_template["id"], node_idx, cluster_id)
                        udlink_id = udlink()
                        procs()

            def router():
                """
                Gateway for inter-cluster connections.
                """

                xml.SubElement(cluster_xml, "router",
                    attrib={"id": "rou_{}".format(cluster_idx)})

            def backbone():
                """
                Intra-cluster connections.
                """

                _backbone_attrs = {"id": "bbo_{}".format(cluster_idx)}
                _backbone_attrs.update(network_types[cluster["cluster_network"]])
                xml.SubElement(cluster_xml, "backbone", attrib=_backbone_attrs)

            cluster_idx = 0
            recorded_nodes = {}
            for cluster in platform["clusters"]:
                cluster_id = "clu_{}".format(cluster_idx)
                cluster_xml = xml.SubElement(main_zone_xml, "zone",
                    attrib={"id": cluster_id, "routing": "Cluster"})
                nodes()
                router()
                backbone()
                cluster_idx += 1

        def global_links():
            """
            Links from clusters to the master zone.
            """

            for cluster_idx in range(len(platform["clusters"])):
                _global_link_attrs = {"id": "tomh_clu_{}".format(cluster_idx)}
                _global_link_attrs.update(network_types[platform["dc_network"]])
                xml.SubElement(main_zone_xml, "link", attrib=_global_link_attrs)

        def routes():
            """
            Routes over global links.
            """

            for cluster_idx in range(len(platform["clusters"])):
                route_xml = xml.SubElement(main_zone_xml, "zoneRoute",
                    attrib={"src": "clu_{}".format(cluster_idx), "dst": "master",
                        "gw_src": "rou_{}".format(cluster_idx), "gw_dst": "master_host"})
                xml.SubElement(route_xml, "link_ctn",
                    attrib={"id": "tomh_clu_{}".format(cluster_idx)})

        platform_xml = xml.Element("platform",
            attrib={"version": "4.1"})
        main_zone_xml = main_zone()
        master_zone()
        config_node_xml = config_node()
        clusters()
        global_links()
        routes()
        return platform_xml

    def write_result():
        """
        Writes the Batsim formatted platform.
        """

        def doctype():
            """
            Provides SimGrid doctype.
            """

            return "<!DOCTYPE platform SYSTEM \"https://simgrid.org/simgrid.dtd\">"

        with open(args.output_xml, "w", ) as output_f:
            output_f.write(xml_format.parseString("{}{}".format(doctype(),
                xml.tostring(xml_tree).decode())).toprettyxml(indent="  ",
                    encoding="utf-8").decode())

    # Command line arguments
    args = cmd_args()
    # User's defined platform and type data
    platform, network_types, node_types, processor_types = load_data()
    # Resulting XML tree
    xml_tree = generate_tree()
    # Write result to the output file
    write_result()

if __name__ == "__main__":
    main()
