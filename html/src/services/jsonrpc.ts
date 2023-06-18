import useSWR from "swr";
import prefixPath from "../base";
import useAuth from "../contexts/auth";

export class RpcError extends Error {
  data: any;
  code: number;

  constructor(code: number, message: string, data: any) {
    super(message)
    this.name = "RpcError";
    this.code = code;
    this.data = data;
  }
}

const fetcher = function<T>(token: string) {
  return async (method: string, params: any, ...args: any[]) => {
    return await jsonrpc<T>(token, method, params);
  };
}

export class AuthError extends Error {
  constructor(message: string) {
    super();
    this.message = message;
  }
}

export async function jsonrpc<T>(token: string, method: string, params?: any) {
  const res = await fetch(prefixPath('/api/v1/jsonrpc'), {
    body: JSON.stringify({
      jsonrpc: '2.0',
      method,
      id: Date.now(),
      params: params || {}
    }),
    headers: {
      Authorization: `Bearer ${token}`
    },
    method: 'POST'
  });

  if (res.status === 401) {
    throw new AuthError(res.statusText);
  }

  if (res.status !== 200) {
    throw new Error(res.statusText);
  }

  const data = await res.json();

  if (data.error) {
    throw new RpcError(data.error.code, data.error.message, data.error.data);
  }

  return data.result as T;
}

export function useInvoker<T>(method: string) {
  const { token } = useAuth();
  if (!token) throw new Error("Invalid token");
  return (params?: any) => jsonrpc<T>(token, method, params);
}

export function useRPC<T>(method: string, params?: any, config?: any) {
  const { token } = useAuth();
  if (!token) throw new Error("Invalid token");
  return useSWR(() => [method, typeof params === "function" ? params() : params], fetcher<T>(token), config);
}
