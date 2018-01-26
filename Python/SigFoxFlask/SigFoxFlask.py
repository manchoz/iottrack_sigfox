#!/usr/bin/env python3
"""Example microservice for SigFox -> Flask -> InfluxDB streaming pipeline.

"""

import struct
import sys

import click

from flask import Flask, request
from influxdb import InfluxDBClient
from influxdb.client import InfluxDBClientError
from requests.exceptions import ConnectionError

import conversions

app = Flask(__name__)


class MyFrame:
    """ Helper class for loading and unloading data from SigFox packet.
    """
    FMT_STR = '<BhHhB'

    DHT_TEMP_MIN = -40
    DHT_TEMP_MAX = 120
    DHT_HUMI_MIN = 0
    DHT_HUMI_MAX = 100
    ATA_TEMP_MIN = -60
    ATA_TEMP_MAX = 60

    def __init__(self):
        self._struct = struct.Struct(self.FMT_STR)
        self.fields = dict(status=None, dhtTemp=None, dhtHumi=None, ataTemp=None, lastMsg=None)

    def unpack(self, data):
        return self._struct.unpack(data)

    def load(self, hex_data):
        # 01320abe14280a00
        # 0x01 0x32 0x0a 0xbe 0x14 0x28 0x0a 0x00
        # [0x01, 0x32, 0x0a, 0xbe, 0x14, 0x28, 0x0a, 0x00]
        array = bytearray.fromhex(hex_data)
        try:
            # s, t, h, a, l = self.unpack(array)
            s, t, h, a, l = struct.unpack('<BhHhB', array)

            self.fields = {
                'status': s,
                'dhtTemp': conversions.int16_to_float(t, self.DHT_TEMP_MAX, self.DHT_TEMP_MIN),
                'dhtHumi': conversions.uint16_to_float(h, self.DHT_HUMI_MAX, self.DHT_HUMI_MIN),
                'ataTemp': conversions.int16_to_float(a, self.ATA_TEMP_MAX, self.ATA_TEMP_MIN),
                'lastMsg': l
            }
        except struct.error as e:
            print(f'Error decoding data {hex_data}: {e}')

    def __repr__(self):
        from pprint import pformat
        return pformat(self.fields)


@app.route('/')
def hello_world():
    return 'Hello World!'


@app.route('/data/<device>/up', methods=['POST'])
def get_data(device):

    msg = request.json
    print(f'Received message {msg} from {device}')

    hex_data = msg['data']
    frame = MyFrame()
    frame.load(hex_data)

    data_point = {
        'measurement': 'environment',
        'time': msg['time'] * (10 ** 9),
        'fields': frame.fields
    }

    msg.pop('data')
    msg.pop('time')
    data_point['tags'] = msg

    print(f'Writing data: {data_point}')
    influx = app.config['INFLUX_CLIENT']
    influx.write_points([data_point])

    return f'Data for device {device} received.'


@click.command()
@click.option('--influx-host', default='localhost', help='Host of InfluxDB server.')
@click.option('--influx-db', default='sigfox', help='Default InfluxDB database.')
def main(influx_host, influx_db):

    influx = InfluxDBClient(host=influx_host, database=influx_db)
    try:
        influx.create_database(influx_db)
    except InfluxDBClientError:
        print(f'Database {influx_db} already exists. Skipping.')
    except ConnectionError:
        print(f'Error connecting to "{influx_host}". Please check hostname.')
        sys.exit(1)

    app.config.update(dict(INFLUX_CLIENT=influx))
    app.run()


if __name__ == '__main__':
    main()
