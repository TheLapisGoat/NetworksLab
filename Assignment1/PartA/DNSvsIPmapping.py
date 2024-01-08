import dpkt

mapping = {}
def process_dns(packet):

    try:
        eth = dpkt.ethernet.Ethernet(packet)
        ip = eth.data
        udp = ip.data
        dns = dpkt.dns.DNS(udp.data)

        if dns.qr == 1:  # DNS response (qr=1)
            for an in dns.an:
                name = an.name.decode("utf-8") if isinstance(an.name, bytes) else an.name
                if name not in mapping:
                    mapping[name] = []

                if an.type == 1:  # IPv4 address
                    ip_address = dpkt.utils.inet_to_str(an.rdata)
                    if ip_address not in mapping[name]:
                        mapping[name].append(ip_address)
                elif an.type == 5:  # CNAME
                    cname = an.cname.decode('utf-8') if isinstance(an.cname, bytes) else an.cname
                    if cname not in mapping[name]:
                        mapping[name].append(cname)

    except Exception as e:
        # Handle exceptions gracefully
        print(f"Error processing packet: {e}")

def process_pcap(file_path):
    with open(file_path, 'rb') as f:
        pcap = dpkt.pcap.Reader(f)
        for timestamp, buf in pcap:
            process_dns(buf)

if __name__ == "__main__":
    pcap_file = "dns.pcap"
    process_pcap(pcap_file)

    for name, values in mapping.items():
        print(f"{name:80}| {' '.join(values)}")

