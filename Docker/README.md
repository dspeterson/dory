# Dory Dockerfile


## Running Dory as Docker

To run dory as docker all you need to do is:
 
 1. Build the docker image using the dockerfile attached
 2. Run the docker with shared volume to other dockers
 3. Launch your Application's docker (the ones using Dory) with shared volume to the socket file

### build the dockerfile

``` docker build -t <host you docker repo>/dory:latest . ```

### Run the docker with shared volume

``` docker run -d --name dory -p 9090:9090 -v /root/ <host you docker repo>/dory ```

sharing the /root volume will enable other dockers sending messages to dory to use this shared volume for the socket file

### Launch your Application's docker with shared volume to the socket file

``` docker run -d --name myapp --volumes-from dory -e DORY_SOCKET=/root/dory.socket <host you docker repo>/myapp:latest ```



