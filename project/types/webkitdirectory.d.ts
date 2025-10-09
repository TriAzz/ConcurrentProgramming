// Type definitions for webkitdirectory attribute
interface HTMLInputElement {
  webkitdirectory?: boolean;
}

declare module 'react' {
  interface InputHTMLAttributes<T> {
    webkitdirectory?: boolean;
  }
}
