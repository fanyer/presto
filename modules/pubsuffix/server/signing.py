import signer

def PerformBodySigning(body_prefix, body, params):
	return signer.SignPostfix(body_prefix, "-->\n" + body, params['keyfile'], params['alg']);
	return body_prefix + "-->\n" + body;