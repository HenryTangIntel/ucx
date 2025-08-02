# Test with Gaudi device memory for both send and receive
ucx_perftest put_lat -m gaudi

# Test with host send memory and Gaudi receive memory  
ucx_perftest put_lat -m host,gaudi

# Test with Gaudi send memory and host receive memory
ucx_perftest put_lat -m gaudi,host
