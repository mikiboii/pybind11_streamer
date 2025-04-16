import asyncio

from TikTokLive.client.client import TikTokLiveClient
from TikTokLive.client.logger import LogLevel
from TikTokLive.events import ConnectEvent, DisconnectEvent

import json

client: TikTokLiveClient = TikTokLiveClient(
    unique_id="@stef_gamingg"
)


@client.on(ConnectEvent)
async def on_connect(event: ConnectEvent):
    client.logger.info("Connected!")

    # print(client.room_info)

    mm = json.loads(client.room_info['stream_url']['live_core_sdk_data']['pull_data']['stream_data'])
    # print(mm['data'])
    formatted_json = json.dumps(mm['data']['origin'], indent=4)  # indent=4 adds pretty-printing
    print(formatted_json)
    # Start a recording
    # client.web.fetch_video_data.start(
    #     output_fp=f"{event.unique_id}.mp4",
    #     room_info=client.room_info,
    #     output_format="mp4"
    # )

    # await asyncio.sleep(5)

    # # Stop the client, we're done!
    # await client.disconnect()


@client.on(DisconnectEvent)
async def on_live_end(_: DisconnectEvent):
    """Stop the download when we disconnect"""

    client.logger.info("Disconnected!")

    if client.web.fetch_video_data.is_recording:
        client.web.fetch_video_data.stop()


if __name__ == '__main__':
    # Enable download info
    client.logger.setLevel(LogLevel.INFO.value)

    # Need room info to download stream
    client.run(fetch_room_info=True)