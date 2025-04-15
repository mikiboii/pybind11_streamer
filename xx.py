# import example

# print(example.say_hello("ssss"))

# https://pull-f5-tt03.tiktokcdn.com/game/stream-3287609553826874245_uhd.flv?_session_id=053-202504151753407D619A5489D15902462A.1744710841949&_webnoredir=1&expire=1745920420&sign=e215f0e817c6a9f45ce0df1877efbb22


from mymodule import Stream_demo

input_url = "https://pull-f5-tt03.tiktokcdn.com/game/stream-3287609553826874245_uhd.flv?_session_id=053-202504151753407D619A5489D15902462A.1744710841949&_webnoredir=1&expire=1745920420&sign=e215f0e817c6a9f45ce0df1877efbb22"
output_url = "rtmp://live-lax.twitch.tv/app/live_1072101235_ztWGwxq7oMHGHkVmsrbqDIGvTV5DW2"
    
frame_count = 0

app = Stream_demo(input_url, output_url)


print("started streaming...")
while True:
    
    try:
        result = app.send_stream()
                
        if result == -1:  # End of stream
            print("End of stream reached")
            break
        elif result == 1:  # Video frame
            self.frame_count += 1
            if self.frame_count % 100 == 0:
                print(f"Sent {self.frame_count} frames")
      
        
    except Exception as e:
            print(f"Error during streaming: {e}")

# 
# stubgen -o . -p mymodule